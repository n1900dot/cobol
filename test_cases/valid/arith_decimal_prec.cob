       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-DECIMAL-PREC.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Decimal Arithmetic and Precision (V clause)
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-DECIMAL-NUMS.
           05  WS-DEC1            PIC S9(04)V99 VALUE 123.45.
           05  WS-DEC2            PIC S9(04)V99 VALUE 67.89.
           05  WS-DEC3            PIC S9(04)V999 VALUE 10.125.
           05  WS-DEC4            PIC S9(04)V999 VALUE 3.333.
       01  WS-RESULTS.
           05  WS-RES-ADD         PIC S9(06)V99 VALUE ZERO.
           05  WS-RES-SUB         PIC S9(06)V99 VALUE ZERO.
           05  WS-RES-MUL         PIC S9(08)V9999 VALUE ZERO.
           05  WS-RES-DIV         PIC S9(06)V9999 VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Decimal Precision Tests ===".
           
           ADD WS-DEC1 TO WS-DEC2 GIVING WS-RES-ADD.
           DISPLAY "123.45 + 67.89 = " WS-RES-ADD.
           
           SUBTRACT WS-DEC2 FROM WS-DEC1 GIVING WS-RES-SUB.
           DISPLAY "123.45 - 67.89 = " WS-RES-SUB.
           
           MULTIPLY WS-DEC3 BY WS-DEC4 GIVING WS-RES-MUL.
           DISPLAY "10.125 * 3.333 = " WS-RES-MUL.
           
           DIVIDE WS-DEC1 BY WS-DEC3 GIVING WS-RES-DIV.
           DISPLAY "123.45 / 10.125 = " WS-RES-DIV.
           
           STOP RUN.
