       IDENTIFICATION DIVISION.
       PROGRAM-ID. FILE-SEQ-WRITE.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT EMP-FILE ASSIGN TO "EMP.DAT"
               ORGANIZATION IS SEQUENTIAL
               ACCESS MODE IS SEQUENTIAL
               FILE STATUS IS WS-FILE-STATUS.
       DATA DIVISION.
       FILE SECTION.
       FD  EMP-FILE.
       01  EMP-RECORD.
           05  EMP-ID        PIC 9(5).
           05  EMP-NAME      PIC X(20).
           05  EMP-SALARY    PIC 9(7)V99.
       WORKING-STORAGE SECTION.
       01  WS-FILE-STATUS    PIC XX.
       01  WS-EOF-FLAG       PIC 9 VALUE 0.
           88 END-OF-FILE    VALUE 1.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           OPEN OUTPUT EMP-FILE
           IF WS-FILE-STATUS NOT = "00"
               DISPLAY "ERROR OPENING FILE: " WS-FILE-STATUS
               STOP RUN
           END-IF
           
           MOVE 10001 TO EMP-ID
           MOVE "JOHN DOE         " TO EMP-NAME
           MOVE 50000.00 TO EMP-SALARY
           WRITE EMP-RECORD
           
           MOVE 10002 TO EMP-ID
           MOVE "JANE SMITH       " TO EMP-NAME
           MOVE 62000.50 TO EMP-SALARY
           WRITE EMP-RECORD
           
           CLOSE EMP-FILE
           DISPLAY "FILE WRITE COMPLETE".
           STOP RUN.
