/*
 * Read-only cycle counter custom instruction.
 *
 * CI result returns a free-running 32-bit counter in the CPU clock domain.
 * The software uses deltas between two reads for diagnostics only; it does not
 * use this counter to control program flow.
 */
module cycleCounterIse #(parameter [7:0] customInstructionId = 8'd9)
                        (input wire        clock,
                                           reset,
                         input wire [7:0]  ciN,
                         input wire        ciStart,
                                           ciCke,
                         output wire       ciDone,
                         output wire [31:0] ciResult);

  reg [31:0] s_cycleCounterReg;
  wire s_isMyCi = (ciN == customInstructionId) ? ciStart & ciCke : 1'b0;

  always @(posedge clock)
    s_cycleCounterReg <= (reset == 1'b1) ? 32'd0 : s_cycleCounterReg + 32'd1;

  assign ciDone = s_isMyCi;
  assign ciResult = (s_isMyCi == 1'b1) ? s_cycleCounterReg : 32'd0;

endmodule
