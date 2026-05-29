module or1420SingleCore ( input wire         clock12MHz,
                                             clock50MHz,
                                             nReset,
                          input wire         RxD,
                          output wire        TxD,

                          output wire        sdramClk,
                          output wire        sdramCke,
                                             sdramCsN,
                                             sdramRasN,
                                             sdramCasN,
                                             sdramWeN,
                          output wire [1:0]  sdramDqmN,
                          output wire [12:0] sdramAddr,
                          output wire [1:0]  sdramBa,
                          inout wire [15:0]  sdramData,

                          // The spi interface
                          output wire        spiScl,
                                             spiNCs,
                          inout wire         spiSiIo0,
                                             spiSoIo1,
                                             spiIo2,
                                             spiIo3,

                          output wire        pixelClock,
                                             horizontalSync,
                                             verticalSync,
                                             activePixel,
                          
`ifdef GECKO5Education
                          output wire [4:0]  hdmiRed,
                                             hdmiBlue,
                                             hdmiGreen,
`else
                          output wire [3:0]  hdmiRed,
                                             hdmiGreen,
                                             hdmiBlue,
                          output wire        camnReset,
`endif
                          output wire        SCL,
                          inout wire         SDA,
                          input wire         camPclk,
                                             camHsync,
                                             camVsync,
                                             biosBypass,
                          input wire [7:0]   camData
                           );

  wire        s_busIdle, s_snoopableBurst;
  wire        s_hdmiDone, s_systemClock, s_systemClockX2, s_swapByteDone, s_flashDone, s_cpuFreqDone;
  wire [31:0] s_hdmiResult, s_swapByteResult, s_flashResult, s_cpuFreqResult;
  wire [5:0]  s_memoryDistance = 6'd0;
  wire        s_busError, s_beginTransaction, s_endTransaction;
  wire [31:0] s_addressData;
  wire [3:0]  s_byteEnables;
  wire        s_readNotWrite, s_dataValid, s_busy;
  wire [7:0]  s_burstSize;
  
  
  /*
   *
   * We use a PLL to generate the required clocks for the HDMI part
   *
   */
  reg[4:0] s_resetCountReg;
  wire     s_pixelClock;
  wire     s_pixelClkX2;
  wire     s_pllLocked;
  wire     s_reset = ~s_resetCountReg[4];

`ifndef GECKO5Education  
  assign camnReset = s_resetCountReg[4];
`endif

  always @(posedge s_systemClock or negedge s_pllLocked)
    if (s_pllLocked == 1'b0) s_resetCountReg <= 5'd0;
    else s_resetCountReg <= (s_resetCountReg[4] == 1'b0) ? s_resetCountReg + 5'd1 : s_resetCountReg;
  
`ifdef GECKO5Education
  wire s_resetPll = ~nReset;
  wire s_feedbackClock;
  localparam CPU_CLOCK_FREQUENCY=74250000;
  // CPU @ 74.25MHz
  EHXPLLL #(
        .PLLRST_ENA("ENABLED"),
        .INTFB_WAKE("DISABLED"),
        .STDBY_ENABLE("DISABLED"),
        .DPHASE_SOURCE("DISABLED"),
        .OUTDIVIDER_MUXA("DIVA"),
        .OUTDIVIDER_MUXB("DIVB"),
        .OUTDIVIDER_MUXC("DIVC"),
        .OUTDIVIDER_MUXD("DIVD"),
        .CLKI_DIV(1),
        .CLKOP_ENABLE("ENABLED"),
        .CLKOP_DIV(4),
        .CLKOP_CPHASE(1),
        .CLKOP_FPHASE(0),
        .CLKOS_ENABLE("ENABLED"),
        .CLKOS_DIV(8),
        .CLKOS_CPHASE(1),
        .CLKOS_FPHASE(0),
        .CLKOS2_ENABLE("ENABLED"),
        .CLKOS2_DIV(8),
        .CLKOS2_CPHASE(1),
        .CLKOS2_FPHASE(0),
        .CLKOS3_ENABLE("ENABLED"),
        .CLKOS3_DIV(4),
        .CLKOS3_CPHASE(1),
        .CLKOS3_FPHASE(0),
        .FEEDBK_PATH("INT_OP"),
        .CLKFB_DIV(2)
    ) pll_1 (
        .RST(s_resetPll),
        .STDBY(1'b0),
        .CLKI(clock12MHz),
        .CLKOP(s_pixelClkX2),
        .CLKOS(s_pixelClock),
        .CLKOS2(s_systemClock),
        .CLKOS3(s_systemClockX2),
        .CLKFB(s_feedbackClock),
        .CLKINTFB(s_feedbackClock),
        .PHASESEL0(1'b0),
        .PHASESEL1(1'b0),
        .PHASEDIR(1'b1),
        .PHASESTEP(1'b1),
        .PHASELOADREG(1'b1),
        .PLLWAKESYNC(1'b0),
        .ENCLKOP(1'b0),
        .LOCK(s_pllLocked)
	);
`else
  wire[4:0] s_pllClocks;
  localparam CPU_CLOCK_FREQUENCY=42428571;
  assign s_pixelClock = s_pllClocks[0];
  assign s_pixelClkX2 = s_pllClocks[1];
  assign s_systemClock = s_pllClocks[2];
  assign s_systemClockX2 = s_pllClocks[3];

  
	altpll	altpll_component (
				.areset (~nReset),
				.inclk ({1'b0,clock12MHz}),
				.clk (s_pllClocks),
				.locked (s_pllLocked),
				.activeclock (),
				.clkbad (),
				.clkena ({6{1'b1}}),
				.clkloss (),
				.clkswitch (1'b0),
				.configupdate (1'b0),
				.enable0 (),
				.enable1 (),
				.extclk (),
				.extclkena ({4{1'b1}}),
				.fbin (1'b1),
				.fbmimicbidir (),
				.fbout (),
				.fref (),
				.icdrclk (),
				.pfdena (1'b1),
				.phasecounterselect ({4{1'b1}}),
				.phasedone (),
				.phasestep (1'b1),
				.phaseupdown (1'b1),
				.pllena (1'b1),
				.scanaclr (1'b0),
				.scanclk (1'b0),
				.scanclkena (1'b1),
				.scandata (1'b0),
				.scandataout (),
				.scandone (),
				.scanread (1'b0),
				.scanwrite (1'b0),
				.sclkout0 (),
				.sclkout1 (),
				.vcooverrange (),
				.vcounderrange ());
	defparam
		altpll_component.bandwidth_type = "AUTO",
		altpll_component.clk0_divide_by = 16,
		altpll_component.clk0_duty_cycle = 50,
		altpll_component.clk0_multiply_by = 99,
		altpll_component.clk0_phase_shift = "0",
		altpll_component.clk1_divide_by = 8,
		altpll_component.clk1_duty_cycle = 50,
		altpll_component.clk1_multiply_by = 99,
		altpll_component.clk1_phase_shift = "0",
		altpll_component.clk2_divide_by = 16,
		altpll_component.clk2_duty_cycle = 50,
		altpll_component.clk2_multiply_by = 99,
		altpll_component.clk2_phase_shift = "0",
		altpll_component.clk3_divide_by = 8,
		altpll_component.clk3_duty_cycle = 50,
		altpll_component.clk3_multiply_by = 99,
		altpll_component.clk3_phase_shift = "0",
		altpll_component.compensate_clock = "CLK0",
		altpll_component.inclk0_input_frequency = 83333,
		altpll_component.intended_device_family = "Cyclone IV E",
		altpll_component.lpm_hint = "CBX_MODULE_PREFIX=test",
		altpll_component.lpm_type = "altpll",
		altpll_component.operation_mode = "NORMAL",
		altpll_component.pll_type = "AUTO",
		altpll_component.port_activeclock = "PORT_UNUSED",
		altpll_component.port_areset = "PORT_USED",
		altpll_component.port_clkbad0 = "PORT_UNUSED",
		altpll_component.port_clkbad1 = "PORT_UNUSED",
		altpll_component.port_clkloss = "PORT_UNUSED",
		altpll_component.port_clkswitch = "PORT_UNUSED",
		altpll_component.port_configupdate = "PORT_UNUSED",
		altpll_component.port_fbin = "PORT_UNUSED",
		altpll_component.port_inclk0 = "PORT_USED",
		altpll_component.port_inclk1 = "PORT_UNUSED",
		altpll_component.port_locked = "PORT_USED",
		altpll_component.port_pfdena = "PORT_UNUSED",
		altpll_component.port_phasecounterselect = "PORT_UNUSED",
		altpll_component.port_phasedone = "PORT_UNUSED",
		altpll_component.port_phasestep = "PORT_UNUSED",
		altpll_component.port_phaseupdown = "PORT_UNUSED",
		altpll_component.port_pllena = "PORT_UNUSED",
		altpll_component.port_scanaclr = "PORT_UNUSED",
		altpll_component.port_scanclk = "PORT_UNUSED",
		altpll_component.port_scanclkena = "PORT_UNUSED",
		altpll_component.port_scandata = "PORT_UNUSED",
		altpll_component.port_scandataout = "PORT_UNUSED",
		altpll_component.port_scandone = "PORT_UNUSED",
		altpll_component.port_scanread = "PORT_UNUSED",
		altpll_component.port_scanwrite = "PORT_UNUSED",
		altpll_component.port_clk0 = "PORT_USED",
		altpll_component.port_clk1 = "PORT_USED",
		altpll_component.port_clk2 = "PORT_USED",
		altpll_component.port_clk3 = "PORT_USED",
		altpll_component.port_clk4 = "PORT_UNUSED",
		altpll_component.port_clk5 = "PORT_UNUSED",
		altpll_component.port_clkena0 = "PORT_UNUSED",
		altpll_component.port_clkena1 = "PORT_UNUSED",
		altpll_component.port_clkena2 = "PORT_UNUSED",
		altpll_component.port_clkena3 = "PORT_UNUSED",
		altpll_component.port_clkena4 = "PORT_UNUSED",
		altpll_component.port_clkena5 = "PORT_UNUSED",
		altpll_component.port_extclk0 = "PORT_UNUSED",
		altpll_component.port_extclk1 = "PORT_UNUSED",
		altpll_component.port_extclk2 = "PORT_UNUSED",
		altpll_component.port_extclk3 = "PORT_UNUSED",
		altpll_component.self_reset_on_loss_lock = "OFF",
		altpll_component.width_clock = 5;

`endif

  /*
   * Here we instantiate the UART
   *
   */
  wire s_uartIrq, s_uartEndTransaction, s_uartDataValid, s_uartBusError;
  wire [31:0] s_uartAddressData;
  uartBus #( .baseAddress(32'h50000000) ) uart1
           ( .clock(s_systemClock),
             .clock_50MHz(clock50MHz),
             .reset(s_reset),
             .irq(s_uartIrq),
             .beginTransactionIn(s_beginTransaction),
             .endTransactionIn(s_endTransaction),
             .readNWriteIn(s_readNotWrite),
             .dataValidIn(s_dataValid),
             .busyIn(s_busy),
             .addressDataIn(s_addressData),
             .byteEnablesIn(s_byteEnables),
             .burstSizeIn(s_burstSize),
             .addressDataOut(s_uartAddressData),
             .endTransactionOut(s_uartEndTransaction),
             .dataValidOut(s_uartDataValid),
             .busErrorOut(s_uartBusError),
             .RxD(RxD),
             .TxD(TxD));

  /*
   * Here we instantiate the SDRAM controller
   *
   */
  wire        s_sdramInitBusy, s_sdramEndTransaction, s_sdramDataValid;
  wire        s_sdramBusy, s_sdramBusError;
  wire [31:0] s_sdramAddressData;
  wire        s_cpuReset = s_reset | s_sdramInitBusy;
  
  sdramController #( .baseAddress(32'h00000000),
                     .systemClockInHz(CPU_CLOCK_FREQUENCY)) sdram
                   ( .clock(s_systemClock),
                     .clockX2(s_systemClockX2),
                     .reset(s_reset),
                     .memoryDistanceIn(s_memoryDistance),
                     .sdramDelay(`ifdef GECKO5Education 2'd2 `else 2'd0 `endif),
                     .sdramInitBusy(s_sdramInitBusy),
                     .beginTransactionIn(s_beginTransaction),
                     .endTransactionIn(s_endTransaction),
                     .readNotWriteIn(s_readNotWrite),
                     .dataValidIn(s_dataValid),
                     .busErrorIn(s_busError),
                     .busyIn(s_busy),
                     .addressDataIn(s_addressData),
                     .byteEnablesIn(s_byteEnables),
                     .burstSizeIn(s_burstSize),
                     .endTransactionOut(s_sdramEndTransaction),
                     .dataValidOut(s_sdramDataValid),
                     .busyOut(s_sdramBusy),
                     .busErrorOut(s_sdramBusError),
                     .addressDataOut(s_sdramAddressData),
                     .sdramClk(sdramClk),
                     .sdramCke(sdramCke),
                     .sdramCsN(sdramCsN),
                     .sdramRasN(sdramRasN),
                     .sdramCasN(sdramCasN),
                     .sdramWeN(sdramWeN),
                     .sdramDqmN(sdramDqmN),
                     .sdramAddr(sdramAddr),
                     .sdramBa(sdramBa),
                     .sdramData(sdramData));

  /*
   * Here we instantiate the CPU
   *
   */
  wire [31:0] s_cpu1CiResult, s_profileResult;
  wire [31:0] s_cpu1CiDataA, s_cpu1CiDataB, s_camCiResult, s_delayResult, s_motion4Result, s_cycleCounterResult;
  wire [7:0]  s_cpu1CiN;
  wire        s_cpu1CiRa, s_cpu1CiRb, s_cpu1CiRc, s_cpu1CiStart, s_cpu1CiCke, s_cpu1CiDone, s_i2cCiDone, s_delayCiDone, s_motion4Done, s_cycleCounterDone;
  wire [4:0]  s_cpu1CiA, s_cpu1CiB, s_cpu1CiC;
  wire        s_cpu1IcacheRequestBus, s_cpu1DcacheRequestBus, s_camCiDone;
  wire        s_cpu1IcacheBusAccessGranted, s_cpu1DcacheBusAccessGranted;
  wire        s_cpu1BeginTransaction, s_cpu1EndTransaction, s_cpu1ReadNotWrite;
  wire [31:0] s_cpu1AddressData, s_i2cCiResult;
  wire [3:0]  s_cpu1byteEnables;
  wire        s_cpu1DataValid;
  wire [7:0]  s_cpu1BurstSize;
  wire        s_spm1Irq, s_stall, s_profileDone;
  
  assign s_cpu1CiDone = s_hdmiDone | s_swapByteDone | s_flashDone | s_cpuFreqDone | s_i2cCiDone | s_delayCiDone | s_camCiDone | s_motion4Done | s_cycleCounterDone | s_profileDone;
  assign s_cpu1CiResult = s_hdmiResult | s_swapByteResult | s_flashResult | s_cpuFreqResult | s_i2cCiResult | s_camCiResult | s_delayResult | s_motion4Result | s_cycleCounterResult | s_profileResult; 

  or1420Top #( .NOP_INSTRUCTION(32'h1500FFFF)) cpu1
             (.cpuClock(s_systemClock),
              .cpuReset(s_cpuReset),
              .irq(1'b0),
              .cpuIsStalled(s_stall),
              .iCacheReqBus(s_cpu1IcacheRequestBus),
              .dCacheReqBus(s_cpu1DcacheRequestBus),
              .iCacheBusGrant(s_cpu1IcacheBusAccessGranted),
              .dCacheBusGrant(s_cpu1DcacheBusAccessGranted),
              .busErrorIn(s_busError),
              .busyIn(s_busy),
              .beginTransActionOut(s_cpu1BeginTransaction),
              .addressDataIn(s_addressData),
              .addressDataOut(s_cpu1AddressData),
              .endTransactionIn(s_endTransaction),
              .endTransactionOut(s_cpu1EndTransaction),
              .byteEnablesOut(s_cpu1byteEnables),
              .dataValidIn(s_dataValid),
              .dataValidOut(s_cpu1DataValid),
              .readNotWriteOut(s_cpu1ReadNotWrite),
              .burstSizeOut(s_cpu1BurstSize),
              .ciStart(s_cpu1CiStart),
              .ciReadRa(s_cpu1CiRa),
              .ciReadRb(s_cpu1CiRb),
              .ciWriteRd(s_cpu1CiRc),
              .ciN(s_cpu1CiN),
              .ciA(s_cpu1CiA),
              .ciB(s_cpu1CiB),
              .ciD(s_cpu1CiC),
              .ciDataA(s_cpu1CiDataA),
              .ciDataB(s_cpu1CiDataB),
              .ciResult(s_cpu1CiResult),
              .ciDone(s_cpu1CiDone));
              
              assign s_cpu1CiCke = 1'b1;

  /*
   *
   * Here we define a custom instruction that determines the cpu-frequency
   *
   *
   */
  wire [31:0] s_cpuFreqValue;
  assign s_cpuFreqDone = (s_cpu1CiN == 8'd4) ? s_cpu1CiStart : 1'b0;
  assign s_cpuFreqResult = (s_cpu1CiN == 8'd4 && s_cpu1CiStart == 1'b1) ? s_cpuFreqValue : 32'd0;
  
  processorId #( .processorId(1),
                 .NumberOfProcessors(1),
                 .ReferenceClockFrequencyInHz(50000000) ) cpuFreq
               ( .clock(s_systemClock),
                 .reset(s_cpuReset),
                 .referenceClock(clock50MHz),
                 .biosBypass(biosBypass),
                 .procFreqId(s_cpuFreqValue) );

  /*
   *
   * Here we define a custom instruction that swaps bytes
   *
   */
  swapByte #(.customIntructionNr(8'd1)) ise1
            (.ciN(s_cpu1CiN),
             .ciDataA(s_cpu1CiDataA),
             .ciDataB(s_cpu1CiDataB),
             .ciStart(s_cpu1CiStart),
             .ciCke(s_cpu1CiCke),
             .ciDone(s_swapByteDone),
             .ciResult(s_swapByteResult));

  /*
   * Custom instruction for software-side movement detection.
   * It compares four packed edge pixels from two frames and returns a
   * changed-pixel mask plus a small population count.
   */
  motion4Ise #(.customInstructionId(8'd8)) motion4
              (.ciN(s_cpu1CiN),
               .ciDataA(s_cpu1CiDataA),
               .ciDataB(s_cpu1CiDataB),
               .ciStart(s_cpu1CiStart),
               .ciCke(s_cpu1CiCke),
               .ciDone(s_motion4Done),
               .ciResult(s_motion4Result));

  cycleCounterIse #(.customInstructionId(8'd9)) cycleCounter
                  (.clock(s_systemClock),
                   .reset(s_cpuReset),
                   .ciN(s_cpu1CiN),
                   .ciStart(s_cpu1CiStart),
                   .ciCke(s_cpu1CiCke),
                   .ciDone(s_cycleCounterDone),
                   .ciResult(s_cycleCounterResult));
  /*
   *
   * Here we define a custom instruction that implements a simple I2C interface
   *
   */
  i2cCustomInstr #(.CLOCK_FREQUENCY(CPU_CLOCK_FREQUENCY),
                   .I2C_FREQUENCY(400000),
                   .CUSTOM_ID(8'd5)) i2cm
                  (.clock(s_systemClock),
                   .reset(s_cpuReset),
                   .ciStart(s_cpu1CiStart),
                   .ciCke(s_cpu1CiCke),
                   .ciN(s_cpu1CiN),
                   .ciOppA(s_cpu1CiDataA),
                   .ciDone(s_i2cCiDone),
                   .result(s_i2cCiResult),
                   .SDA(SDA),
                   .SCL(SCL));

  /*
   *
   * Here we define a custom instruction that implements a blocking micro-second(s) delay element
   *
   */
  delayIse #(.referenceClockFrequencyInHz(50000000),
             .customInstructionId(8'd6) ) delayMicro
            (.clock(s_systemClock),
             .referenceClock(clock50MHz),
             .reset(s_cpuReset),
             .ciStart(s_cpu1CiStart),
             .ciCke(s_cpu1CiCke),
             .ciN(s_cpu1CiN),
             .ciValueA(s_cpu1CiDataA),
             .ciValueB(s_cpu1CiDataB),
             .ciDone(s_delayCiDone),
             .ciResult(s_delayResult));

  /*
   *
   * Here we define a profile ISE
   *
   */
   profileCi #(.customId(8'd12)) profiler
              (.start(s_cpu1CiStart),
               .clock(s_systemClock),
               .reset(s_cpuReset),
               .stall(s_stall),
               .busIdle(s_busIdle),
               .valueA(s_cpu1CiDataA),
               .valueB(s_cpu1CiDataB),
               .ciN(s_cpu1CiN),
               .done(s_profileDone),
               .result(s_profileResult));
  /*
   *
   * Here we define the camera interface
   *
   */
  wire s_camReqBus, s_camAckBus, s_camBeginTransaction, s_camEndTransaction;
  wire s_camDataValid;
  wire [31:0] s_camAddressData;
  wire [3:0] s_camByteEnables;
  wire [7:0] s_camBurstSize;
  
  camera #(.customInstructionId(8'd7),
           .clockFrequencyInHz(CPU_CLOCK_FREQUENCY)) camIf
          (.clock(s_systemClock),
           .pclk(camPclk),
           .reset(s_cpuReset),
           .hsync(camHsync),
           .vsync(camVsync),
           .ciStart(s_cpu1CiStart),
           .ciCke(s_cpu1CiCke),
           .ciN(s_cpu1CiN),
           .camData(camData),
           .ciValueA(s_cpu1CiDataA),
           .ciValueB(s_cpu1CiDataB),
           .ciResult(s_camCiResult),
           .ciDone(s_camCiDone),
           .requestBus(s_camReqBus),
           .busGrant(s_camAckBus),
           .beginTransactionOut(s_camBeginTransaction),
           .addressDataOut(s_camAddressData),
           .endTransactionOut(s_camEndTransaction),
           .byteEnablesOut(s_camByteEnables),
           .dataValidOut(s_camDataValid),
           .burstSizeOut(s_camBurstSize),
           .busyIn(s_busy),
           .busErrorIn(s_busError));

  
  /*
   *
   * Here the hdmi controller is defined
   *
   */
  wire        s_hdmiRequestBus, s_hdmiBusgranted, s_hdmiBeginTransaction;
  wire        s_hdmiEndTransaction, s_hdmiDataValid, s_hdmiReadNotWrite;
  wire [3:0]  s_hdmiByteEnables;
  wire [7:0]  s_hdmiBurstSize;
  wire [31:0] s_hdmiAddressData;

  screens #(.baseAddress(32'h50000020),
            .pixelClockFrequency(27'd74250000),
            .cursorBlinkFrequency(27'd1)) hdmi 
           (.pixelClockIn(s_pixelClock),
            .clock(s_systemClock),
            .reset(s_reset),
            .testPicture(1'b0),
            .nrOfScreens(2'd1),
            .ci1N(s_cpu1CiN),
            .ci1DataA(s_cpu1CiDataA),
            .ci1DataB(s_cpu1CiDataB),
            .ci1Start(s_cpu1CiStart),
            .ci1Cke(s_cpu1CiCke),
            .ci1Done(s_hdmiDone),
            .ci1Result(s_hdmiResult),
            .ci2N(8'd0),
            .ci2DataA(32'd0),
            .ci2DataB(32'd0),
            .ci2Start(1'b0),
            .ci2Cke(1'b0),
            .ci2Done(),
            .ci2Result(),
            .ci3N(8'd0),
            .ci3DataA(32'd0),
            .ci3DataB(32'd0),
            .ci3Start(1'b0),
            .ci3Cke(1'b0),
            .ci3Done(),
            .ci3Result(),
            .requestTransaction(s_hdmiRequestBus),
            .transactionGranted(s_hdmiBusgranted),
            .beginTransactionIn(s_beginTransaction),
            .endTransactionIn(s_endTransaction),
            .readNotWriteIn(s_readNotWrite),
            .dataValidIn(s_dataValid),
            .busErrorIn(s_busError),
            .addressDataIn(s_addressData),
            .byteEnablesIn(s_byteEnables),
            .burstSizeIn(s_burstSize),
            .beginTransactionOut(s_hdmiBeginTransaction),
            .endTransactionOut(s_hdmiEndTransaction),
            .dataValidOut(s_hdmiDataValid),
            .readNotWriteOut(s_hdmiReadNotWrite),
            .byteEnablesOut(s_hdmiByteEnables),
            .burstSizeOut(s_hdmiBurstSize),
            .addressDataOut(s_hdmiAddressData),
            .pixelClkX2(s_pixelClkX2),
`ifdef GECKO5Education
            .hdmiRed(hdmiRed),
            .hdmiGreen(hdmiGreen),
            .hdmiBlue(hdmiBlue),
`else
            .red(hdmiRed),
            .green(hdmiGreen),
            .blue(hdmiBlue),
`endif
            .pixelClock(pixelClock),
            .horizontalSync(horizontalSync),
            .verticalSync(verticalSync),
            .activePixel(activePixel)
            );

  /*
   *
   * Here the spi-flash controller is defined
   *
   */
  wire [31:0] s_flashAddressData;
  wire s_flashEndTransaction, s_flashDataValid, s_flashBusError;
  spiBus #( .baseAddress(32'h04000000),
            .customIntructionNr(8'd2)) flash
          ( .clock(s_systemClock),
            .reset(s_reset),
            .spiScl(spiScl),
            .spiNCs(spiNCs),
            .spiSiIo0(spiSiIo0),
            .spiSoIo1(spiSoIo1),
            .spiIo2(spiIo2),
            .spiIo3(spiIo3),
            .ciN(s_cpu1CiN),
            .ciDataA(s_cpu1CiDataA),
            .ciDataB(s_cpu1CiDataB),
            .ciStart(s_cpu1CiStart),
            .ciCke(s_cpu1CiCke),
            .ciDone(s_flashDone),
            .ciResult(s_flashResult),
            .beginTransactionIn(s_beginTransaction),
            .endTransactionIn(s_endTransaction),
            .readNotWriteIn(s_readNotWrite),
            .busErrorIn(s_busError),
            .addressDataIn(s_addressData),
            .burstSizeIn(s_burstSize),
            .byteEnablesIn(s_byteEnables),
            .addressDataOut(s_flashAddressData),
            .endTransactionOut(s_flashEndTransaction),
            .dataValidOut(s_flashDataValid),
            .busErrorOut(s_flashBusError) );

  /*
   *
   * Here we define the bios
   *
   */
  wire [31:0] s_biosAddressData;
  wire        s_biosBusError, s_biosDataValid, s_biosEndTransaction;
  bios start (.clock(s_systemClock),
              .reset(s_reset),
              .addressDataIn(s_addressData),
              .beginTransactionIn(s_beginTransaction),
              .endTransactionIn(s_endTransaction),
              .readNotWriteIn(s_readNotWrite),
              .busErrorIn(s_busError),
              .dataValidIn(s_dataValid),
              .byteEnablesIn(s_byteEnables),
              .burstSizeIn(s_burstSize),
              .addressDataOut(s_biosAddressData),
              .busErrorOut(s_biosBusError),
              .dataValidOut(s_biosDataValid),
              .endTransactionOut(s_biosEndTransaction));

  /*
   *
   * Here we define the bus arbiter
   *
   */
 wire [31:0] s_busRequests, s_busGrants;
 wire        s_arbBusError, s_arbEndTransaction;
 
 assign s_busRequests[31] = s_cpu1DcacheRequestBus;
 assign s_busRequests[30] = s_cpu1IcacheRequestBus;
 assign s_busRequests[29] = s_hdmiRequestBus;
 assign s_busRequests[28] = s_camReqBus;
 assign s_busRequests[27:0] = 28'd0;
 
 assign s_cpu1DcacheBusAccessGranted = s_busGrants[31];
 assign s_cpu1IcacheBusAccessGranted = s_busGrants[30];
 assign s_hdmiBusgranted             = s_busGrants[29];
 assign s_camAckBus                  = s_busGrants[28];

 busArbiter arbiter ( .clock(s_systemClock),
                      .reset(s_reset),
                      .busRequests(s_busRequests),
                      .busGrants(s_busGrants),
                      .busErrorOut(s_arbBusError),
                      .endTransactionOut(s_arbEndTransaction),
                      .busIdle(s_busIdle),
                      .snoopableBurst(s_snoopableBurst),
                      .beginTransactionIn(s_beginTransaction),
                      .endTransactionIn(s_endTransaction),
                      .dataValidIn(s_dataValid),
                      .addressDataIn(s_addressData[31:30]),
                      .burstSizeIn(s_burstSize));
 
  /*
   *
   * Here we define the bus architecture
   *
   */
 assign s_busError         = s_arbBusError | s_biosBusError | s_uartBusError | s_sdramBusError | s_flashBusError;
 assign s_beginTransaction = s_cpu1BeginTransaction | s_hdmiBeginTransaction | s_camBeginTransaction;
 assign s_endTransaction   = s_cpu1EndTransaction | s_arbEndTransaction | s_biosEndTransaction | s_uartEndTransaction |
                             s_sdramEndTransaction | s_hdmiEndTransaction | s_flashEndTransaction | s_camEndTransaction;
 assign s_addressData      = s_cpu1AddressData | s_biosAddressData | s_uartAddressData | s_sdramAddressData | s_hdmiAddressData |
                             s_flashAddressData | s_camAddressData;
 assign s_byteEnables      = s_cpu1byteEnables | s_hdmiByteEnables | s_camByteEnables;
 assign s_readNotWrite     = s_cpu1ReadNotWrite | s_hdmiReadNotWrite;
 assign s_dataValid        = s_cpu1DataValid | s_biosDataValid | s_uartDataValid | s_sdramDataValid | s_hdmiDataValid | 
                             s_flashDataValid | s_camDataValid;
 assign s_busy             = s_sdramBusy;
 assign s_burstSize        = s_cpu1BurstSize | s_hdmiBurstSize | s_camBurstSize;
 
endmodule
