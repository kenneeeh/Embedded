module testbench ( input wire clock,
                              clockX2,
                              reset );

  wire sdramInitBusy;
  wire beginTransactionIn, endTransactionIn, readNotWriteIn, dataValidIn, busyIn;
  wire endTransactionOut, dataValidOut, busyOut;
  wire [31:0] addressDataIn, addressDataOut;
  wire [3:0]  byteEnablesIn;
  wire [7:0]  burstSizeIn;
  
  sdramController dut ( .clock(clock),
                    .clockX2(clockX2),
                    .reset(reset),
                    .memoryDistanceIn(6'd0),
                    .sdramInitBusy(sdramInitBusy),
                    .beginTransactionIn(beginTransactionIn),
                    .endTransactionIn(endTransactionIn),
                    .readNotWriteIn(readNotWriteIn),
                    .dataValidIn(dataValidIn),
                    .busErrorIn(1'b0),
                    .busyIn(busyIn),
                    .addressDataIn(addressDataIn),
                    .byteEnablesIn(byteEnablesIn),
                    .burstSizeIn(burstSizeIn),
                    .endTransactionOut(endTransactionOut),
                    .dataValidOut(dataValidOut),
                    .busyOut(busyOut),
                    .busErrorOut(),
                    .addressDataOut(addressDataOut));

  reg beginTransactionIn1, endTransactionIn1, readNotWriteIn1, dataValidIn1, busyIn1;
  reg [31:0] addressDataIn1;
  reg [3:0]  byteEnablesIn1;
  reg [7:0]  burstSizeIn1;
  
  assign beginTransactionIn = beginTransactionIn1;
  assign endTransactionIn   = endTransactionIn1 | endTransactionOut;
  assign readNotWriteIn     = readNotWriteIn1;
  assign dataValidIn        = dataValidIn1 | dataValidOut;
  assign busyIn             = busyIn1 | busyOut;
  assign addressDataIn      = addressDataIn1;
  assign byteEnablesIn      = byteEnablesIn1;
  assign burstSizeIn        = burstSizeIn1;
  
  reg [31:0] s_state, s_next;
  
  always @(posedge clock) s_state <= (reset == 1'b1) ? 0 : s_next;
  
  always @*
    begin
      beginTransactionIn1 <= 1'b0;
      endTransactionIn1   <= 1'b0;
      readNotWriteIn1     <= 1'b0;
      dataValidIn1        <= 1'b0;
      busyIn1             <= 1'b0;
      addressDataIn1      <= 32'd0;
      byteEnablesIn1      <= 4'd0;
      burstSizeIn1        <= 8'd0;
      case (s_state)
        0       : s_next <= (sdramInitBusy == 1'b1) ? 0 : 1;
        1       : s_next <= 2;
        2       : begin
                    beginTransactionIn1 <= 1'b1;
                    byteEnablesIn1      <= 4'hF;
                    burstSizeIn1        <= 8'd8;
                    s_next              <= 3;
                  end
        3       : begin
                    dataValidIn1   <= 1'b1;
                    addressDataIn1 <= 32'h01234567;
                    s_next         <= (busyIn == 1'b1) ? 3 : 4;
                  end
        4       : begin
                    dataValidIn1   <= 1'b1;
                    addressDataIn1 <= 32'h89ABCDEF;
                    s_next         <= (busyIn == 1'b1) ? 4 : 5;
                  end
        5       : begin
                    dataValidIn1   <= 1'b1;
                    addressDataIn1 <= 32'hAABB5577;
                    s_next         <= (busyIn == 1'b1) ? 5 : 6;
                  end
        6       : begin
                    endTransactionIn1 <= 1'b1;
                    s_next            <= 7;
                  end
        7       : begin
                    beginTransactionIn1 <= 1'b1;
                    byteEnablesIn1      <= 4'hF;
                    readNotWriteIn1     <= 1'b1;
                    addressDataIn1      <= 32'h3E8;
                    burstSizeIn1        <= 8'd15;
                    s_next              <= 8;
                  end
        8       : s_next <= (endTransactionIn == 1'b1) ? 9 : 8;
        9       : begin
                    beginTransactionIn1 <= 1'b1;
                    byteEnablesIn1      <= 4'h8;
                    addressDataIn1      <= 32'd1024;
                    s_next              <= 10;
                  end
        10      : begin
                    dataValidIn1   <= 1'b1;
                    addressDataIn1 <= 32'h67676767;
                    s_next         <= (busyIn == 1'b1) ? 10 : 11;
                  end
        11      : begin
                    endTransactionIn1 <= 1'b1;
                    s_next            <= 12;
                  end
        default : s_next <= s_state;
      endcase
    end

endmodule
