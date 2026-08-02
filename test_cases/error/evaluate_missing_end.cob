       IDENTIFICATION DIVISION.
       PROGRAM-ID. INVALID-EVALUATE-SYNTAX.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-VALUE PIC 9 VALUE 5.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
      * Error: EVALUATE without END-EVALUATE
           EVALUATE WS-VALUE
               WHEN 1
                   DISPLAY "ONE"
               WHEN 5
                   DISPLAY "FIVE"
           STOP RUN.
