       IDENTIFICATION DIVISION.
       PROGRAM-ID. LOOP-TEST.
       
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WS-COUNTER PIC 99 VALUE 0.
       01 WS-SUM PIC 9(5) VALUE 0.
       
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           PERFORM VARYING WS-COUNTER FROM 1 BY 1 
                   UNTIL WS-COUNTER > 5
               COMPUTE WS-SUM = WS-SUM + WS-COUNTER
               DISPLAY "COUNTER: " WS-COUNTER " SUM: " WS-SUM
           END-PERFORM.
           
           MOVE 0 TO WS-COUNTER.
           PERFORM UNTIL WS-COUNTER >= 3
               ADD 1 TO WS-COUNTER
               DISPLAY "UNTIL LOOP: " WS-COUNTER
           END-PERFORM.
           
           MOVE 0 TO WS-COUNTER.
           PERFORM WITH TEST AFTER UNTIL WS-COUNTER >= 3
               ADD 1 TO WS-COUNTER
               DISPLAY "TEST AFTER: " WS-COUNTER
           END-PERFORM.
           
           STOP RUN.
