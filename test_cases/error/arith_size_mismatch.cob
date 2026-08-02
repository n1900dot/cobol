       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-ERR-SIZE-MISMATCH.
       AUTHOR. TEST-GENERATOR.
      * Error Case: Size Mismatch in Signed/Unsigned Operation
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-NUMS.
           05  WS-TINY            PIC 9(02) VALUE 99.
           05  WS-HUGE            PIC 9(10) VALUE 1234567890.
       01  WS-RESULT.
           05  WS-RES             PIC 9(05) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "Size mismatch test (Tiny + Huge into Medium)...".
           ADD WS-TINY TO WS-HUGE GIVING WS-RES.
           DISPLAY "Likely Truncated Result: " WS-RES.
           STOP RUN.
