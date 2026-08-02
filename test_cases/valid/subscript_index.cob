       IDENTIFICATION DIVISION.
       PROGRAM-ID. SUBSCRIPT-INDEX.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-TABLE-SUB.
           05  WS-NUMBER PIC 9(3) OCCURS 10 TIMES.
       01  WS-TABLE-IDX.
           05  WS-VALUE  PIC 9(3) OCCURS 10 TIMES 
                         INDEXED BY WS-IDX.
       01  WS-SUB          PIC 9(2).
       01  WS-SUM-SUB      PIC 9(5) VALUE 0.
       01  WS-SUM-IDX      PIC 9(5) VALUE 0.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           DISPLAY "=== SUBSCRIPT OPERATIONS ===".
           PERFORM VARYING WS-SUB FROM 1 BY 1 UNTIL WS-SUB > 5
               COMPUTE WS-NUMBER(WS-SUB) = WS-SUB * 10
               DISPLAY "SUB(" WS-SUB "): " WS-NUMBER(WS-SUB)
               ADD WS-NUMBER(WS-SUB) TO WS-SUM-SUB
           END-PERFORM
           DISPLAY "SUM USING SUBSCRIPT: " WS-SUM-SUB.
           
           DISPLAY "=== INDEX OPERATIONS ===".
           SET WS-IDX TO 1.
           PERFORM UNTIL WS-IDX > 5
               COMPUTE WS-VALUE(WS-IDX) = WS-IDX * 20
               DISPLAY "IDX(" WS-IDX "): " WS-VALUE(WS-IDX)
               ADD WS-VALUE(WS-IDX) TO WS-SUM-IDX
               SET WS-IDX UP BY 1
           END-PERFORM
           DISPLAY "SUM USING INDEX: " WS-SUM-IDX.
           
           DISPLAY "=== MIXED ACCESS ===".
           MOVE 3 TO WS-SUB.
           SET WS-IDX TO 3.
           DISPLAY "SUB(3): " WS-NUMBER(WS-SUB).
           DISPLAY "IDX(3): " WS-VALUE(WS-IDX).
           
           SET WS-IDX DOWN BY 1.
           DISPLAY "AFTER DOWN BY 1, IDX: " WS-IDX 
                   " VALUE: " WS-VALUE(WS-IDX).
           
           STOP RUN.
