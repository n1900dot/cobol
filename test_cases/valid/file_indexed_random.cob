       IDENTIFICATION DIVISION.
       PROGRAM-ID. FILE-RANDOM-ACCESS.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT EMP-FILE ASSIGN TO "EMP.IDX"
               ORGANIZATION IS INDEXED
               ACCESS MODE IS RANDOM
               FILE STATUS IS WS-FILE-STATUS
               RECORD KEY IS EMP-ID.
       DATA DIVISION.
       FILE SECTION.
       FD  EMP-FILE.
       01  EMP-RECORD.
           05  EMP-ID        PIC 9(5).
           05  EMP-NAME      PIC X(20).
           05  EMP-SALARY    PIC 9(7)V99.
       WORKING-STORAGE SECTION.
       01  WS-FILE-STATUS    PIC XX.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           OPEN I-O EMP-FILE
           IF WS-FILE-STATUS NOT = "00"
               DISPLAY "ERROR OPENING FILE: " WS-FILE-STATUS
               STOP RUN
           END-IF
           
           MOVE 10001 TO EMP-ID
           MOVE "ALICE JOHNSON    " TO EMP-NAME
           MOVE 75000.00 TO EMP-SALARY
           WRITE EMP-RECORD
               INVALID KEY
                   DISPLAY "DUPLICATE KEY FOUND"
               NOT INVALID KEY
                   DISPLAY "RECORD WRITTEN SUCCESSFULLY"
           END-WRITE
           
           MOVE 10001 TO EMP-ID
           READ EMP-FILE
               INVALID KEY
                   DISPLAY "RECORD NOT FOUND"
               NOT INVALID KEY
                   DISPLAY "FOUND: " EMP-NAME " SALARY: " EMP-SALARY
           END-READ
           
           CLOSE EMP-FILE
           STOP RUN.
