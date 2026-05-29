/*
 * motion4Ise.v
 *
 * Custom instruction for the Sobel movement detector project.
 * It compares four packed 8-bit pixels in ciDataA and ciDataB and returns:
 *   result[3:0] = per-byte change mask
 *   result[6:4] = number of changed bytes, 0..4
 *
 * This is useful when the streaming camera accelerator writes a binary Sobel
 * edge image.  The CPU compares the current edge frame with the previous edge
 * frame four pixels at a time and accumulates motion statistics in software.
 */
module motion4Ise #(parameter [7:0] customInstructionId = 8'd8)
                   (input wire [7:0]   ciN,
                    input wire [31:0]  ciDataA,
                                       ciDataB,
                    input wire         ciStart,
                                       ciCke,
                    output wire        ciDone,
                    output wire [31:0] ciResult);

  wire s_isMyCi = (ciN == customInstructionId) ? ciStart & ciCke : 1'b0;

  wire s_change0 = |(ciDataA[7:0]   ^ ciDataB[7:0]);
  wire s_change1 = |(ciDataA[15:8]  ^ ciDataB[15:8]);
  wire s_change2 = |(ciDataA[23:16] ^ ciDataB[23:16]);
  wire s_change3 = |(ciDataA[31:24] ^ ciDataB[31:24]);

  wire [3:0] s_mask = {s_change3, s_change2, s_change1, s_change0};
  wire [2:0] s_count = {2'd0,s_change0} + {2'd0,s_change1} +
                       {2'd0,s_change2} + {2'd0,s_change3};

  assign ciDone   = s_isMyCi;
  assign ciResult = (s_isMyCi == 1'b1) ? {25'd0, s_count, s_mask} : 32'd0;

endmodule
