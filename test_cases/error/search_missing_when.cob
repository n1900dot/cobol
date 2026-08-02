       IDENTIFICATION DIVISION.
       PROGRAM-ID. INVALID-SEARCH-SYNTAX.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  WS-TABLE.
           05  WS-ENTRY OCCURS 5 TIMES INDEXED BY WS-IDX.
               10  WS-CODE PIC 9(3).
       01  WS-SEARCH-VAL PIC 9(3).
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           MOVE 100 TO WS-SEARCH-VAL.
      * Error: SEARCH without WHEN clause is invalid
           SEARCH WS-ENTRY
               AT END
                   DISPLAY "NOT FOUND"
           END-SEARCH.
           STOP RUN.
