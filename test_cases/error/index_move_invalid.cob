       IDENTIFICATION DIVISION.
       PROGRAM-ID. INVALID-INDEX-SET.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-TABLE.
           05  WS-NUMBER PIC 9(3) OCCURS 10 TIMES 
                         INDEXED BY WS-IDX.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
      * Error: Cannot MOVE to an index, must use SET
           MOVE 5 TO WS-IDX.
           DISPLAY WS-NUMBER(WS-IDX).
           STOP RUN.
