       IDENTIFICATION DIVISION.
       PROGRAM-ID. SEARCH-TEST.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-TABLE-SEARCH.
           05  WS-ENTRY OCCURS 5 TIMES INDEXED BY WS-IDX.
               10  WS-CODE     PIC 9(3).
               10  WS-DESC     PIC X(15).
       01  WS-FOUND-FLAG       PIC 9 VALUE 0.
           88 FOUND           VALUE 1.
           88 NOT-FOUND       VALUE 0.
       01  WS-SEARCH-CODE      PIC 9(3).
       PROCEDURE DIVISION.
       INIT-TABLE.
           MOVE 101 TO WS-CODE(1)
           MOVE "APPLE" TO WS-DESC(1)
           MOVE 205 TO WS-CODE(2)
           MOVE "BANANA" TO WS-DESC(2)
           MOVE 307 TO WS-CODE(3)
           MOVE "CHERRY" TO WS-DESC(3)
           MOVE 412 TO WS-CODE(4)
           MOVE "DATE" TO WS-DESC(4)
           MOVE 599 TO WS-CODE(5)
           MOVE "ELDERBERRY" TO WS-DESC(5).
       
       MAIN-PROCEDURE.
           PERFORM INIT-TABLE.
           
           DISPLAY "=== LINEAR SEARCH TEST ===".
           MOVE 307 TO WS-SEARCH-CODE.
           SET WS-IDX TO 1.
           SET FOUND TO FALSE.
           
           SEARCH WS-ENTRY
               AT END
                   DISPLAY "CODE " WS-SEARCH-CODE " NOT FOUND"
               WHEN WS-CODE(WS-IDX) = WS-SEARCH-CODE
                   SET FOUND TO TRUE
                   DISPLAY "FOUND: " WS-CODE(WS-IDX) 
                           " - " WS-DESC(WS-IDX)
           END-SEARCH.
           
           DISPLAY "=== BINARY SEARCH TEST ===".
           MOVE 412 TO WS-SEARCH-CODE.
           SET WS-IDX TO 1.
           SET FOUND TO FALSE.
           
           SEARCH ALL WS-ENTRY
               AT END
                   DISPLAY "CODE " WS-SEARCH-CODE " NOT FOUND"
               WHEN WS-CODE(WS-IDX) = WS-SEARCH-CODE
                   SET FOUND TO TRUE
                   DISPLAY "FOUND: " WS-CODE(WS-IDX) 
                           " - " WS-DESC(WS-IDX)
           END-SEARCH.
           
           STOP RUN.
