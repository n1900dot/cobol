       IDENTIFICATION DIVISION.
       PROGRAM-ID. EVALUATE-WITH-TEST.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-COUNTER   PIC 9(3) VALUE 1.
       01  WS-RESULT    PIC X(20) VALUE SPACES.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           EVALUATE TRUE
               WHEN WS-COUNTER > 5
                   MOVE "HIGH" TO WS-RESULT
               WHEN WS-COUNTER > 2
                   MOVE "MEDIUM" TO WS-RESULT
               WHEN OTHER
                   MOVE "LOW" TO WS-RESULT
           END-EVALUATE.
           
           EVALUATE WS-COUNTER
               WHEN 1 THRU 3
                   MOVE "RANGE 1-3" TO WS-RESULT
               WHEN 4 THRU 6
                   MOVE "RANGE 4-6" TO WS-RESULT
               WHEN OTHER
                   MOVE "OUT OF RANGE" TO WS-RESULT
           END-EVALUATE.
           
           EVALUATE TRUE
               WHEN WS-COUNTER = 1
                   MOVE "ONE" TO WS-RESULT
               WHEN WS-COUNTER = 2
                   MOVE "TWO" TO WS-RESULT
               WHEN OTHER
                   MOVE "OTHER" TO WS-RESULT
           END-EVALUATE.
           
           STOP RUN.
