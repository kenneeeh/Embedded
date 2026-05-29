module profileCi #( parameter[7:0] customId = 8'h00 )
                  ( input wire        start,
                                      clock,
                                      reset,
                                      stall,
                                      busIdle,
                    input wire [31:0] valueA,
                                      valueB,
                    input wire [7:0]  ciN,
                    output wire       done,
                    output reg [31:0] result );

  wire [31:0] s_counterValue0, s_counterValue1, s_counterValue2, s_counterValue3;
  wire s_isMyCi = (ciN == customId) ? start : 1'b0;
  
  assign done = s_isMyCi;
  
  reg s_enableCounter0, s_enableCounter1, s_enableCounter2, s_enableCounter3;
  wire s_resetCounter0 = (reset == 1'b1 || (valueB[8] == 1'b1)) ? s_isMyCi : 1'b0;
  wire s_resetCounter1 = (reset == 1'b1 || (valueB[9] == 1'b1)) ? s_isMyCi : 1'b0;
  wire s_resetCounter2 = (reset == 1'b1 || (valueB[10] == 1'b1)) ? s_isMyCi : 1'b0;
  wire s_resetCounter3 = (reset == 1'b1 || (valueB[11] == 1'b1)) ? s_isMyCi : 1'b0;

  always @(posedge clock)
    begin
      s_enableCounter0 <= (reset == 1'b1 || (valueB[4] == 1'b1 && s_isMyCi == 1'b1)) ? 1'b0 :
                          (valueB[0] == 1'b1 && s_isMyCi == 1'b1) ? 1'b1 : s_enableCounter0;
      s_enableCounter1 <= (reset == 1'b1 || (valueB[5] == 1'b1 && s_isMyCi == 1'b1)) ? 1'b0 :
                          (valueB[1] == 1'b1 && s_isMyCi == 1'b1) ? 1'b1 : s_enableCounter1;
      s_enableCounter2 <= (reset == 1'b1 || (valueB[6] == 1'b1 && s_isMyCi == 1'b1)) ? 1'b0 :
                          (valueB[2] == 1'b1 && s_isMyCi == 1'b1) ? 1'b1 : s_enableCounter2;
      s_enableCounter3 <= (reset == 1'b1 || (valueB[7] == 1'b1 && s_isMyCi == 1'b1)) ? 1'b0 :
                          (valueB[3] == 1'b1 && s_isMyCi == 1'b1) ? 1'b1 : s_enableCounter3;
    end
  
  counter #(.WIDTH(32)) counter0
           (.reset(s_resetCounter0),
            .clock(clock),
            .enable(s_enableCounter0),
            .direction(1'b1),
            .counterValue(s_counterValue0));

  counter #(.WIDTH(32)) counter1
           (.reset(s_resetCounter1),
            .clock(clock),
            .enable(s_enableCounter1&stall),
            .direction(1'b1),
            .counterValue(s_counterValue1));

  counter #(.WIDTH(32)) counter2
           (.reset(s_resetCounter2),
            .clock(clock),
            .enable(s_enableCounter2&busIdle),
            .direction(1'b1),
            .counterValue(s_counterValue2));

  counter #(.WIDTH(32)) counter3
           (.reset(s_resetCounter3),
            .clock(clock),
            .enable(s_enableCounter3),
            .direction(1'b1),
            .counterValue(s_counterValue3));

  always @*
    if (s_isMyCi == 1'b0) result <= 32'd0;
    else case (valueA[1:0])
      2'd0    : result <= s_counterValue0;
      2'd1    : result <= s_counterValue1;
      2'd2    : result <= s_counterValue2;
      default : result <= s_counterValue3;
    endcase
endmodule
