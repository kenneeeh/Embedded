module rgb565Grayscale( input wire [15:0] rgb565,
                        output wire [7:0] grayscale );

  wire [7:3] s_red = rgb565[15:11];
  wire [7:2] s_green = rgb565[10:5];
  wire [7:3] s_blue = rgb565[4:0];
  
  /* Here we determine red * 54 or red * 00110110b */
  wire [10:4] s_redx2 = {2'd0,s_red};
  wire [10:4] s_redx4 = {1'b0,s_red,1'b0};
  wire [10:4] s_redSum = s_redx2 + s_redx4;
  wire [15:4] s_redSumLo = {5'd0,s_redSum};
  wire [15:4] s_redSumHi = {2'd0,s_redSum,3'd0};
  wire [15:4] s_redResult = s_redSumLo + s_redSumHi;
  
  /* Here we determine blue*19 or blue * 00010011b */
  wire [9:3] s_bluex1 = {2'd0,s_blue};
  wire [9:3] s_bluex2 = {1'b0,s_blue,1'b0};
  wire [9:3] s_blueSum = s_bluex1 + s_bluex2;
  wire [15:3] s_blueLo = {7'd0,s_blueSum};
  wire [15:3] s_blueHi = {3'd0,s_blue,4'd0};
  wire [15:3] s_blueResult = s_blueLo + s_blueHi;
  
  /* Here we determine green*183 or green * 10110111 */
  wire [9:2] s_greenx1 = {2'd0,s_green};
  wire [9:2] s_greenx2 = {1'b0,s_green,1'b0};
  wire [9:2] s_greenSum = s_greenx1 + s_greenx2;
  wire [15:3] s_greenLo = {5'd0,s_greenSum};
  wire [15:3] s_greenHi = {2'd0,s_greenSum,3'd0};
  wire [15:3] s_greenSum1 = s_greenLo + s_greenHi;
  wire [15:3] s_greenx129 = {1'b0,s_green,1'b0,s_green[7:3]}; /* s_green x 10000001b  LSB does not matter anyways */
  wire [15:3] s_greenResult = s_greenSum1 + s_greenx129;
  
  /* finally we make the grayscale */
  wire [15:3] s_rbSum = {s_redResult,1'b0} + s_blueResult;
  wire [15:3] s_rgbSum = s_rbSum + s_greenResult;
  
  assign grayscale = s_rgbSum[15:8];
endmodule
