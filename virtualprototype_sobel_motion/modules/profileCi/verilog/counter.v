module counter #( parameter WIDTH = 8)
                ( input wire reset,
                             clock,
                             enable,
                             direction, /* a 1 is counting up, a 0 is counting down */
                  output reg [WIDTH-1:0] counterValue);

  always @(posedge clock)
    counterValue <= (reset == 1'b1) ? {WIDTH{1'b0}} : 
                    (enable == 1'b0) ? counterValue :
                    (direction == 1'b1) ? counterValue + 1 : counterValue - 1;
endmodule
