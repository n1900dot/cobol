       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-SIGNED-MUL.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Signed Multiplication
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-SIGNED-NUMS.
           05  WS-SIGN-M1         PIC S9(03) VALUE 12.
           05  WS-SIGN-M2         PIC S9(03) VALUE -5.
           05  WS-SIGN-M3         PIC S9(03) VALUE -8.
           05  WS-SIGN-M4         PIC S9(03) VALUE 10.
       01  WS-RESULTS.
           05  WS-RES-MUL1        PIC S9(08) VALUE ZERO.
           05  WS-RES-MUL2        PIC S9(08) VALUE ZERO.
           05  WS-RES-MUL3        PIC S9(08) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Signed Multiplication Tests ===".
           
           MULTIPLY WS-SIGN-M1 BY WS-SIGN-M4 GIVING WS-RES-MUL1.
           DISPLAY "Pos * Pos (12 * 10): " WS-RES-MUL1.
           
           MULTIPLY WS-SIGN-M1 BY WS-SIGN-M2 GIVING WS-RES-MUL2.
           DISPLAY "Pos * Neg (12 * -5): " WS-RES-MUL2.
           
           MULTIPLY WS-SIGN-M2 BY WS-SIGN-M3 GIVING WS-RES-MUL3.
           DISPLAY "Neg * Neg (-5 * -8): " WS-RES-MUL3.
           
           STOP RUN.
