       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-UNSIGNED-SUB.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Unsigned Subtraction (Borrowing, Zero Result)
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-UNSIGNED-NUMS.
           05  WS-UNSIGN-X        PIC 9(05) VALUE 50000.
           05  WS-UNSIGN-Y        PIC 9(05) VALUE 25000.
           05  WS-UNSIGN-Z        PIC 9(05) VALUE 100.
           05  WS-UNSIGN-W        PIC 9(05) VALUE 100.
       01  WS-RESULTS.
           05  WS-RES-SUB1        PIC 9(06) VALUE ZERO.
           05  WS-RES-SUB2        PIC 9(06) VALUE ZERO.
           05  WS-RES-SUB3        PIC 9(06) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Unsigned Subtraction Tests ===".
           
           SUBTRACT WS-UNSIGN-Y FROM WS-UNSIGN-X GIVING WS-RES-SUB1.
           DISPLAY "Large Sub (50000 - 25000): " WS-RES-SUB1.
           
           SUBTRACT WS-UNSIGN-Z FROM WS-UNSIGN-X GIVING WS-RES-SUB2.
           DISPLAY "Diff Sizes (50000 - 100): " WS-RES-SUB2.
           
           SUBTRACT WS-UNSIGN-W FROM WS-UNSIGN-Z GIVING WS-RES-SUB3.
           DISPLAY "Equal Sub (100 - 100): " WS-RES-SUB3.
           
           STOP RUN.
