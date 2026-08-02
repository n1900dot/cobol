       IDENTIFICATION DIVISION.
       PROGRAM-ID. CONDITIONAL-TEST.
       
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WS-AGE PIC 99 VALUE 25.
       01 WS-STATUS PIC X(20).
       
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           IF WS-AGE < 18
               MOVE "MINOR" TO WS-STATUS
               DISPLAY "STATUS: MINOR"
           ELSE
               MOVE "ADULT" TO WS-STATUS
               DISPLAY "STATUS: ADULT"
           END-IF.
           
           EVALUATE WS-AGE
               WHEN 0 THRU 12
                   DISPLAY "CHILD"
               WHEN 13 THRU 19
                   DISPLAY "TEENAGER"
               WHEN 20 THRU 64
                   DISPLAY "ADULT"
               WHEN OTHER
                   DISPLAY "SENIOR"
           END-EVALUATE.
           
           STOP RUN.
