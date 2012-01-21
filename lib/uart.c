/** @file uart.c
*
* @brief UART functions
*
* @author Alvaro Prieto
*/
#include "uart.h"

static uint8_t dummy_callback( uint8_t );

static uint8_t (*uart_rx_callback)( uint8_t ) = dummy_callback;

#if !defined(__MSP430G2533__)
#error This serial library was written for device MSP430G2533
#endif

/*******************************************************************************
 * @fn     void setup_uart( void )
 * @brief  configure uart for 115200BAUD on ports 1.6 and 1.7
 * ****************************************************************************/
void setup_uart( void )
{

  P1SEL   |= BIT1 + BIT2;              // Select UART/SPI function
  P1SEL2  |= BIT1 + BIT2;              // Select UART/SPI function

  // 
  // Configure Serial Port
  // 
  UCA0CTL1 |= UCSWRST;                // **Put state machine in reset**
  UCA0CTL1 |= UCSSEL_2;               // CLK = SMCLK

  //UCA0BR0 = 0xe2;                     // 12MHz/9600=1250 (see User's Guide)
  //UCA0BR1 = 0x04;                     // 1250 = 0x4e2
  //UCA0MCTL = UCBRS_0+UCBRF_0;         // Modulation UCBRSx=0, UCBRFx=0

  UCA0BR0 = 138;                      // 16MHz/115200=138 (see User's Guide)
  UCA0BR1 = 0x00;                     //
  UCA0MCTL = UCBRS_7+UCBRF_0;         // Modulation UCBRSx=3, UCBRFx=0
  UCA0CTL1 &= ~UCSWRST;               // **Initialize USCI state machine**

  IE2 |= UCA0RXIE;                    // Enable USCI_A0/B0 RX interrupt

}

/*******************************************************************************
 * @fn     uart_put_char( uint8_t character )
 * @brief  transmit single character
 * ****************************************************************************/
void uart_put_char( uint8_t character )
{ 
  while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
  UCA0TXBUF = character;
}



/*******************************************************************************
 * @fn     setup_uart_callback( uint8_t (*callback)(uint8_t) )
 * @brief  Register UART Rx Callback function
 * ****************************************************************************/
void setup_uart_callback( uint8_t (*callback)(uint8_t) )
{
  uart_rx_callback = callback;
}

/*******************************************************************************
 * @fn     uart_write( uint8_t character )
 * @brief  transmit whole buffer
 * ****************************************************************************/
void uart_write( uint8_t* buffer, uint16_t length )
{
  uint16_t buffer_index;
  
  for( buffer_index = 0; buffer_index < length; buffer_index++ )
  {

    while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
    UCA0TXBUF = buffer[buffer_index];

  }
}

/*******************************************************************************
 * @fn     uart_write_escaped( uint8_t character )
 * @brief  transmit whole buffer while escaping characters
 * ****************************************************************************/
void uart_write_escaped( uint8_t* buffer, uint16_t length )
{
  uint16_t buffer_index;
    
  while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
  UCA0TXBUF = SYNC_BYTE;

  for( buffer_index = 0; buffer_index < length; buffer_index++ )
  {
    if( (buffer[buffer_index] == SYNC_BYTE) | (buffer[buffer_index] == ESCAPE_BYTE) )
    {
      while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
      UCA0TXBUF = ESCAPE_BYTE;
      while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
      UCA0TXBUF = buffer[buffer_index] ^ 0x20;
    }
    else
    {

      while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
      UCA0TXBUF = buffer[buffer_index];

    }
  }

  while (!(IFG2&UCA0TXIFG));  // USCI_A0 TX buffer ready?
  UCA0TXBUF = SYNC_BYTE;

}

/*******************************************************************************
 * @fn     void dummy_callback( uint8_t rx_char )
 * @brief  empty function works as default callback
 * ****************************************************************************/
static uint8_t dummy_callback( uint8_t rx_char )
{
  __no_operation();
  
  return 0;
}


/*******************************************************************************
 * @fn     void uart_rx_isr( void )
 * @brief  UART ISR
 * ****************************************************************************/
#pragma vector=USCIAB0RX_VECTOR
__interrupt void uart_rx_isr(void) // CHANGE
{  
  // Process incoming byte from USART
  if( IFG2 & UCA0RXIFG )
  { 
    // Call rx callback function
    if( uart_rx_callback( UCA0RXBUF ) )
    {
      // If function returns something nonzero, wakeup the processor
      __bic_SR_register_on_exit(LPM1_bits);
    }
  }
  // Process incoming byte from SPI
  else if ( IFG2 & UCB0RXIFG )
  {
        
  }
}

/*******************************************************************************
 * @fn     void uart_tx_isr( void )
 * @brief  UART ISR
 * ****************************************************************************/
#pragma vector=USCIAB0TX_VECTOR
__interrupt void uart_tx_isr(void) // CHANGE
{
  // Done transmitting character through SPI
  if ( IFG2 & UCB0TXIFG )
  {
      // Check if this is a burst transfer, otherwise, pull CSn high
      //CSn_PxOUT &= ~CSn_PIN;
      
      // Disable TX interrupts on SPI
      //IE2 &= ~UCB0TXIE;
  }
}


/*******************************************************************************
 * @fn     uint8_t hex_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
 *                                          uint8_t buffer_in_size  )
 * @brief  DEBUG function used to convert hex values to [hex]string format
 * ****************************************************************************/
uint8_t hex_to_string( uint8_t* buffer_out, uint8_t* buffer_in, 
                                    uint8_t buffer_in_size  )
{
  static const uint8_t hex_char[16] = { '0', '1', '2', '3', '4', '5', '6', '7', 
                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
  uint8_t counter = 0;
  
  while( counter < buffer_in_size * 2 )
  {
    buffer_out[counter] = hex_char[((buffer_in[(counter>>1)]>>4) & 0xF)];
    counter++;
    buffer_out[counter] = hex_char[(buffer_in[(counter>>1)] & 0xF)];
    counter++;
  }
  
  // Terminate string with null character
  buffer_out[counter++] = 0;
  
  return counter;
}

