// This program will list the duration (in nano seconds) of pulse high and low time
// Low time will be reported between high pulses.
// Any time duration longer than 4 seconds is omitted
// presently a limit of 100 high and 100 low durations will be stored
// use serial input to view durations and also to clear them
volatile uint32_t cc0_array [100];
volatile uint32_t cc1_array [100];
volatile uint32_t isr_cc0_count;
volatile uint32_t isr_cc1_count;
volatile uint32_t a;
volatile uint32_t b;
volatile uint32_t high_duration;
volatile uint32_t low_duration;
volatile uint32_t array_length;
volatile char     serial_command;

#define SerialUSB Serial // this is needed for trinket m0
#define PIN 7            // 7 is PB9 on XIAO, 2 is PA9 on trinket m0, both have pin #9 which is odd 

void setup()
{
  SerialUSB.begin(115200);                       // Send data back on the native port
  while(!SerialUSB);                             // Wait for the SerialUSB port to be ready
 
  REG_PM_APBCMASK |= PM_APBCMASK_EVSYS |         // Switch on the event system peripheral
                     PM_APBCMASK_TC4   |         // Switch on the TC4 peripheral
                     PM_APBCMASK_TC5;            // Switch on the TC5 peripheral

  REG_GCLK_GENDIV = GCLK_GENDIV_DIV(0) |         // Divide the 48MHz system clock by 1 = 48MHz
                    GCLK_GENDIV_ID(1);           // Set division on Generic Clock Generator (GCLK) 1
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |          // Set the duty cycle to 50/50 HIGH/LOW
                     GCLK_GENCTRL_GENEN |        // Enable GCLK
                     GCLK_GENCTRL_SRC_DFLL48M |  // Set the clock source to 48MHz
                     GCLK_GENCTRL_ID(1);         // Set clock source on GCLK 1
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  // second write to CLKCTRL, different ID
  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN      |   // Enable the generic clock
                     GCLK_CLKCTRL_GEN_GCLK1  |   // on GCLK1
                     GCLK_CLKCTRL_ID_TC4_TC5;    // Feed the GCLK1 also to TC4
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  // Enable the port multiplexer on pin number "PIN"
  PORT->Group[g_APinDescription[PIN].ulPort].PINCFG[g_APinDescription[PIN].ulPin].bit.PULLEN = 1; // out is default low so pull-down
  PORT->Group[g_APinDescription[PIN].ulPort].PINCFG[g_APinDescription[PIN].ulPin].bit.INEN   = 1;
  PORT->Group[g_APinDescription[PIN].ulPort].PINCFG[g_APinDescription[PIN].ulPin].bit.PMUXEN = 1;
  // Set-up the pin as an EIC (interrupt) peripheral on an odd pin. "0" means odd, "A" function is EIC
  PORT->Group[g_APinDescription[PIN].ulPort].PMUX[g_APinDescription[PIN].ulPin >> 1].reg |= PORT_PMUX_PMUXO_A;

  EIC->EVCTRL.reg     = EIC_EVCTRL_EXTINTEO9;                           // Enable event output on external interr
  EIC->CONFIG[1].reg  = EIC_CONFIG_SENSE1_HIGH;                         // Set event detecting a high (config 1, #1 is 9
  EIC->INTENCLR.reg   = EIC_INTENCLR_EXTINT9;                           // Clear the interrupt flag on channel 9
  EIC->CTRL.reg       = EIC_CTRL_ENABLE;                                // Enable EIC peripheral
  while (EIC->STATUS.bit.SYNCBUSY);                                     // Wait for synchronization

  REG_EVSYS_CHANNEL = EVSYS_CHANNEL_EDGSEL_NO_EVT_OUTPUT |              // No event edge detection, we already have it on the EIC
                      EVSYS_CHANNEL_PATH_ASYNCHRONOUS    |              // Set event path as asynchronous
                      EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_9) |  // Set event generator (sender) as external interrupt 9
                      EVSYS_CHANNEL_CHANNEL(0);                         // Attach the generator (sender) to channel 0
  
  REG_EVSYS_USER = EVSYS_USER_CHANNEL(1) |                              // Attach the event user (receiver) to channel 0 (n + 1)
                   EVSYS_USER_USER(EVSYS_ID_USER_TC4_EVU);              // Set the event user (receiver) as timer TC4

  REG_TC4_EVCTRL  |= TC_EVCTRL_TCEI |            // Enable the TC event input
                     TC_EVCTRL_EVACT_PWP;        // Set up the timer for capture: CC0 high pulsewidth, CC1 period

  REG_TC4_READREQ = TC_READREQ_RREQ |            // Enable a read request
                    TC_READREQ_ADDR(0x18);       // Offset of the CTRLC register, 32bit
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for (read) synchronization
  
  REG_TC4_CTRLC |= TC_CTRLC_CPTEN1 |             // Enable capture on CC1
                   TC_CTRLC_CPTEN0;              // Enable capture on CC0
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for (write) synchronization

  NVIC_SetPriority(TC4_IRQn, 0);                 // Set Nested Vector Interrupt Controller (NVIC) priority for TC4 to 0 (highest)
  NVIC_EnableIRQ(TC4_IRQn);                      // Connect TC4 timer to the Nested Vector Interrupt Controller (NVIC)

  isr_cc0_count = 0;
  isr_cc1_count = 0;

  REG_TC4_INTENSET = TC_INTENSET_MC1 |           // Enable compare channel 1 (CC1) interrupts
                     TC_INTENSET_MC0;            // Enable compare channel 0 (CC0) interrupts
 
  REG_TC4_CTRLA |= TC_CTRLA_PRESCALER_DIV1 |     // Set prescaler to 1, 48MHz/1 = 48MHz
                   TC_CTRLA_MODE_COUNT32   |     // Set the TC4 timer to 32-bit mode in conjuction with timer TC5
                   TC_CTRLA_ENABLE;              // Enable TC4
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for synchronization
}

void loop()
{

  // wait for user input
  if (SerialUSB.available())
  {
    noInterrupts();
    serial_command = SerialUSB.read();
    // the 'c' character will clear the counts
    if (serial_command == 'c')
    {
      isr_cc0_count = 0;
      isr_cc1_count = 0;
    }
    else
    { 
      if (isr_cc0_count > 0)
      {
        array_length = isr_cc0_count - 1;
        for ( a=0; a < array_length; a = a + 1 )
        {
          // if the duration is close to overflow, remove it from array
          high_duration = (cc0_array[a] * (1000.0 / 48.0) + 21); // convert to nanoseconds, add one extra 48MHz clock period
          if (high_duration > 4000000000)
            {
              // subtract one from the count and left shift overtop the overflow value
              isr_cc0_count--;
              for ( b=a; b < array_length; b = b + 1 )
              {
                cc0_array[b] = cc0_array[b+1];
              }
            }
        }
      }
      if (isr_cc1_count > 0)
      {
        array_length = isr_cc1_count - 1;
        for ( a=0; a < array_length; a = a + 1 )
        {
          // if the duration is close to overflow, remove it from array
          low_duration = (cc1_array[a] * (1000.0 / 48.0) + 21); // convert to nanoseconds, add one extra 48MHz clock period
          if (low_duration > 4000000000)
          {
            // subtract one from the count and left shift overtop the overflow value
            isr_cc1_count--;
            for ( b=a; b < array_length; b = b + 1 )
            {
              cc1_array[b] = cc1_array[b+1];
            }
          }
        }
      }
      SerialUSB.print("pos_pules_count=");
      SerialUSB.println(isr_cc0_count);
      SerialUSB.print("neg_pulse_count=");
      SerialUSB.println(isr_cc1_count);

      for ( a=0; a < isr_cc0_count; a = a + 1 )
      {
        high_duration = cc0_array[a] * (1000.0 / 48.0) + 21;
        SerialUSB.print("high duration=");
        SerialUSB.println(high_duration);
        if (a < isr_cc1_count)
        {
          low_duration = (cc1_array[a] * (1000.0 / 48.0) + 21) - high_duration; // the cc1 count is the period, sum of high and low time
          SerialUSB.print("low duration=");
          SerialUSB.println(low_duration);
          SerialUSB.println();
        }
      }
      SerialUSB.print("--> input c to clear counts <--\n");
      interrupts();
    }
  }
}

// this should be as fast as possible to ensure short durations are not missed
void TC4_Handler()   // Interrupt Service Routine (ISR) for timer TC4
{
  // Check for match counter 0 (MC0) interrupt
  if (TC4->COUNT32.INTFLAG.bit.MC0)             
  {
    cc0_array[isr_cc0_count++] = REG_TC4_COUNT32_CC0;
    REG_TC4_INTFLAG = TC_INTFLAG_MC0; // clear the interrupt
  }
  // Check for match counter 1 (MC1) interrupt
  if (TC4->COUNT32.INTFLAG.bit.MC1)           
  {
    cc1_array[isr_cc1_count++] = REG_TC4_COUNT32_CC1;
    REG_TC4_INTFLAG = TC_INTFLAG_MC1; // clear the interrupt
  }
}
