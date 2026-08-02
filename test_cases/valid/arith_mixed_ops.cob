       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-MIXED-OPS.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Mixed Signed/Unsigned Operations
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-MIXED-NUMS.
           05  WS-SIGN-A          PIC S9(04) VALUE -100.
           05  WS-UNSIGN-B        PIC 9(04) VALUE 50.
           05  WS-SIGN-C          PIC S9(04) VALUE 200.
           05  WS-UNSIGN-D        PIC 9(04) VALUE 25.
       01  WS-RESULTS.
           05  WS-RES-MIX1        PIC S9(08) VALUE ZERO.
           05  WS-RES-MIX2        PIC S9(08) VALUE ZERO.
           05  WS-RES-MIX3        PIC S9(08) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Mixed Signed/Unsigned Tests ===".
           
           ADD WS-SIGN-A TO WS-UNSIGN-B GIVING WS-RES-MIX1.
           DISPLAY "Signed(-100) + Unsigned(50): " WS-RES-MIX1.
           
           MULTIPLY WS-SIGN-C BY WS-UNSIGN-D GIVING WS-RES-MIX2.
           DISPLAY "Signed(200) * Unsigned(25): " WS-RES-MIX2.
           
           SUBTRACT WS-UNSIGN-D FROM WS-SIGN-A GIVING WS-RES-MIX3.
           DISPLAY "Signed(-100) - Unsigned(25): " WS-RES-MIX3.
           
           STOP RUN.
