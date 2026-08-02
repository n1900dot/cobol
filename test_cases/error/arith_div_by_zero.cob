       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-ERR-DIV-ZERO.
       AUTHOR. TEST-GENERATOR.
      * Error Case: Division by Zero (Runtime Error)
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-NUMS.
           05  WS-DIVIDEND        PIC S9(04) VALUE 100.
           05  WS-DIVISOR         PIC S9(04) VALUE 0.
       01  WS-RESULT.
           05  WS-QUOTIENT        PIC S9(06) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "Attempting division by zero...".
           DIVIDE WS-DIVIDEND BY WS-DIVISOR GIVING WS-QUOTIENT.
           DISPLAY "Result: " WS-QUOTIENT.
           STOP RUN.
