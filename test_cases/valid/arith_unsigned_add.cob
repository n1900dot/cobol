       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARITH-UNSIGNED-ADD.
       AUTHOR. TEST-GENERATOR.
      * Test Case: Unsigned Addition (Pure Positive, No Sign Bit)
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-UNSIGNED-NUMS.
           05  WS-UNSIGN-A        PIC 9(05) VALUE 12345.
           05  WS-UNSIGN-B        PIC 9(05) VALUE 67890.
           05  WS-UNSIGN-C        PIC 9(03) VALUE 999.
           05  WS-UNSIGN-D        PIC 9(03) VALUE 1.
       01  WS-RESULTS.
           05  WS-RES-ADD1        PIC 9(06) VALUE ZERO.
           05  WS-RES-ADD2        PIC 9(06) VALUE ZERO.
           05  WS-RES-ADD3        PIC 9(10) VALUE ZERO.
       
       PROCEDURE DIVISION.
       MAIN-PARA.
           DISPLAY "=== Unsigned Addition Tests ===".
           
           ADD WS-UNSIGN-A TO WS-UNSIGN-B GIVING WS-RES-ADD1.
           DISPLAY "Large Add (12345 + 67890): " WS-RES-ADD1.
           
           ADD WS-UNSIGN-C TO WS-UNSIGN-D GIVING WS-RES-ADD2.
           DISPLAY "Small Add (999 + 1): " WS-RES-ADD2.
           
           ADD WS-UNSIGN-A TO WS-UNSIGN-B TO WS-UNSIGN-C 
               GIVING WS-RES-ADD3.
           DISPLAY "Chain Add (12345 + 67890 + 999): " WS-RES-ADD3.
           
           STOP RUN.
