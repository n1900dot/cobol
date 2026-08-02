       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-COMPUTE-EXPR.
       AUTHOR. TEST-GENERATOR.
      * Test Case: COMPUTE Statement with Complex Expressions
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-VARS.
           05  WS-A               PIC S9(04) VALUE 10.
           05  WS-B               PIC S9(04) VALUE 5.
           05  WS-C               PIC S9(04) VALUE 2.
           05  WS-D               PIC S9(04) VALUE 3.
       01  WS-RESULTS.
           05  WS-RES1            PIC S9(08)V99 VALUE ZERO.
           05  WS-RES2            PIC S9(08)V99 VALUE ZERO.
           05  WS-RES3            PIC S9(08)V99 VALUE ZERO.
           05  WS-RES4            PIC S9(08)V99 VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== COMPUTE Expression Tests ===".
           
           COMPUTE WS-RES1 = WS-A + WS-B * WS-C.
           DISPLAY "10 + 5 * 2 = " WS-RES1.
           
           COMPUTE WS-RES2 = (WS-A + WS-B) * WS-C.
           DISPLAY "(10 + 5) * 2 = " WS-RES2.
           
           COMPUTE WS-RES3 = WS-A ** WS-C.
           DISPLAY "10 ** 2 (Power) = " WS-RES3.
           
           COMPUTE WS-RES4 = (WS-A + WS-B) / WS-D * WS-C.
           DISPLAY "(10 + 5) / 3 * 2 = " WS-RES4.
           
           STOP RUN.
