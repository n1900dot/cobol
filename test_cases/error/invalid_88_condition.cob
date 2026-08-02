       IDENTIFICATION DIVISION.
       PROGRAM-ID. INVALID-88-LEVEL.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-FLAG             PIC 9 VALUE 0.
           88 VALID-FLAG       VALUE 1, 2, 3.
           88 INVALID-COND     VALUE "ABC".
      * Error: 88 level cannot have alphanumeric value 
      * when base field is numeric
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           IF VALID-FLAG
               DISPLAY "VALID"
           END-IF
           STOP RUN.
