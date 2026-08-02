       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-SIGNED-SUB.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Signed Subtraction
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-SIGNED-NUMS.
           05  WS-SIGN-A          PIC S9(04) VALUE 5000.
           05  WS-SIGN-B          PIC S9(04) VALUE 2000.
           05  WS-SIGN-NEG-C      PIC S9(04) VALUE -1500.
           05  WS-SIGN-NEG-D      PIC S9(04) VALUE -3000.
       01  WS-RESULTS.
           05  WS-RES-SUB1        PIC S9(06) VALUE ZERO.
           05  WS-RES-SUB2        PIC S9(06) VALUE ZERO.
           05  WS-RES-SUB3        PIC S9(06) VALUE ZERO.
           05  WS-RES-SUB4        PIC S9(06) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Signed Subtraction Tests ===".
           
           SUBTRACT WS-SIGN-B FROM WS-SIGN-A GIVING WS-RES-SUB1.
           DISPLAY "Pos - Pos (5000 - 2000): " WS-RES-SUB1.
           
           SUBTRACT WS-SIGN-A FROM WS-SIGN-B GIVING WS-RES-SUB2.
           DISPLAY "Pos - Pos (2000 - 5000): " WS-RES-SUB2.
           
           SUBTRACT WS-SIGN-NEG-C FROM WS-SIGN-A GIVING WS-RES-SUB3.
           DISPLAY "Pos - Neg (5000 - (-1500)): " WS-RES-SUB3.
           
           SUBTRACT WS-SIGN-NEG-D FROM WS-SIGN-NEG-C 
               GIVING WS-RES-SUB4.
           DISPLAY "Neg - Neg (-1500 - (-3000)): " WS-RES-SUB4.
           
           STOP RUN.
