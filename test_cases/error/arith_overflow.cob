       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-ERR-OVERFLOW.
       AUTHOR. TEST-GENERATOR.
      * Error Case: Arithmetic Overflow (Result too large for PIC)
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-NUMS.
           05  WS-LARGE-A         PIC 9(03) VALUE 900.
           05  WS-LARGE-B         PIC 9(03) VALUE 800.
       01  WS-RESULT.
           05  WS-SMALL-RES       PIC 9(03) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "Attempting overflow (900 + 800 into PIC 9(03))...".
           ADD WS-LARGE-A TO WS-LARGE-B GIVING WS-SMALL-RES.
           DISPLAY "Truncated Result: " WS-SMALL-RES.
           STOP RUN.
