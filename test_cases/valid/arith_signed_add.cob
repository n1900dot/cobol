       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-SIGNED-ADD.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Signed Addition (Positive + Positive, Negative, Mixed)
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-SIGNED-NUMS.
           05  WS-SIGN-POS1      PIC S9(04) VALUE 1000.
           05  WS-SIGN-POS2      PIC S9(04) VALUE 2500.
           05  WS-SIGN-NEG1      PIC S9(04) VALUE -500.
           05  WS-SIGN-NEG2      PIC S9(04) VALUE -1200.
           05  WS-SIGN-MIXED1    PIC S9(04) VALUE 800.
           05  WS-SIGN-MIXED2    PIC S9(04) VALUE -300.
       01  WS-RESULTS.
           05  WS-RES-PP         PIC S9(06) VALUE ZERO.
           05  WS-RES-NN         PIC S9(06) VALUE ZERO.
           05  WS-RES-MIX        PIC S9(06) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Signed Addition Tests ===".
           
           ADD WS-SIGN-POS1 TO WS-SIGN-POS2 GIVING WS-RES-PP.
           DISPLAY "Pos + Pos: " WS-SIGN-POS1 " + " WS-SIGN-POS2 
                   " = " WS-RES-PP.
           
           ADD WS-SIGN-NEG1 TO WS-SIGN-NEG2 GIVING WS-RES-NN.
           DISPLAY "Neg + Neg: " WS-SIGN-NEG1 " + " WS-SIGN-NEG2 
                   " = " WS-RES-NN.
           
           ADD WS-SIGN-MIXED1 TO WS-SIGN-MIXED2 GIVING WS-RES-MIX.
           DISPLAY "Mixed: " WS-SIGN-MIXED1 " + " WS-SIGN-MIXED2 
                   " = " WS-RES-MIX.
           
           STOP RUN.
