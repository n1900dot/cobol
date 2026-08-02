       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-UNSIGNED-MULDIV.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Unsigned Multiplication and Division
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-UNSIGNED-NUMS.
           05  WS-UNSIGN-M1       PIC 9(04) VALUE 250.
           05  WS-UNSIGN-M2       PIC 9(04) VALUE 40.
           05  WS-UNSIGN-D1       PIC 9(06) VALUE 100000.
           05  WS-UNSIGN-D2       PIC 9(04) VALUE 32.
       01  WS-RESULTS.
           05  WS-RES-MUL         PIC 9(08) VALUE ZERO.
           05  WS-RES-DIV         PIC 9(06) VALUE ZERO.
           05  WS-REM-DIV         PIC 9(06) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Unsigned Mul/Div Tests ===".
           
           MULTIPLY WS-UNSIGN-M1 BY WS-UNSIGN-M2 GIVING WS-RES-MUL.
           DISPLAY "Unsigned Mul (250 * 40): " WS-RES-MUL.
           
           DIVIDE WS-UNSIGN-D1 BY WS-UNSIGN-D2 
               GIVING WS-RES-DIV REMAINDER WS-REM-DIV.
           DISPLAY "Unsigned Div (100000 / 32): Q=" WS-RES-DIV
                   " R=" WS-REM-DIV.
           
           STOP RUN.
