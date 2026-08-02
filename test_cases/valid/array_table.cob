       IDENTIFICATION DIVISION.
       PROGRAM-ID. ARRAY-TEST.
       
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 WS-TABLE.
           05 WS-NUMBER PIC 9(3) OCCURS 5 TIMES.
       01 WS-IDX PIC 99.
       01 WS-TOTAL PIC 9(5) VALUE 0.
       
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           MOVE 10 TO WS-NUMBER(1).
           MOVE 20 TO WS-NUMBER(2).
           MOVE 30 TO WS-NUMBER(3).
           MOVE 40 TO WS-NUMBER(4).
           MOVE 50 TO WS-NUMBER(5).
           
           PERFORM VARYING WS-IDX FROM 1 BY 1 
                   UNTIL WS-IDX > 5
               COMPUTE WS-TOTAL = WS-TOTAL + WS-NUMBER(WS-IDX)
               DISPLAY "ELEMENT " WS-IDX ": " WS-NUMBER(WS-IDX)
           END-PERFORM.
           
           DISPLAY "TOTAL: " WS-TOTAL.
           
           STOP RUN.
