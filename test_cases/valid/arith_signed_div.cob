       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-SIGNED-DIV.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Signed Division with Remainder
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-SIGNED-NUMS.
           05  WS-SIGN-D1         PIC S9(04) VALUE 100.
           05  WS-SIGN-D2         PIC S9(04) VALUE -7.
           05  WS-SIGN-D3         PIC S9(04) VALUE -50.
           05  WS-SIGN-D4         PIC S9(04) VALUE 8.
       01  WS-RESULTS.
           05  WS-RES-DIV1        PIC S9(06) VALUE ZERO.
           05  WS-RES-DIV2        PIC S9(06) VALUE ZERO.
           05  WS-REM-DIV1        PIC S9(06) VALUE ZERO.
           05  WS-REM-DIV2        PIC S9(06) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Signed Division Tests ===".
           
           DIVIDE WS-SIGN-D1 BY WS-SIGN-D4 
               GIVING WS-RES-DIV1 REMAINDER WS-REM-DIV1.
           DISPLAY "Pos / Pos (100 / 8): Quotient=" WS-RES-DIV1
                   " Remainder=" WS-REM-DIV1.
           
           DIVIDE WS-SIGN-D1 BY WS-SIGN-D2 
               GIVING WS-RES-DIV2 REMAINDER WS-REM-DIV2.
           DISPLAY "Pos / Neg (100 / -7): Quotient=" WS-RES-DIV2
                   " Remainder=" WS-REM-DIV2.
           
           DIVIDE WS-SIGN-D3 BY WS-SIGN-D2 
               GIVING WS-RES-DIV1 REMAINDER WS-REM-DIV1.
           DISPLAY "Neg / Neg (-50 / -7): Quotient=" WS-RES-DIV1
                   " Remainder=" WS-REM-DIV1.
           
           STOP RUN.
