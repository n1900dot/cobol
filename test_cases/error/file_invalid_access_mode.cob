       IDENTIFICATION DIVISION.
       PROGRAM-ID. INVALID-FILE-MODE.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT EMP-FILE ASSIGN TO "EMP.DAT"
               ORGANIZATION IS INDEXED
               ACCESS MODE IS RANDOM
               FILE STATUS IS WS-STATUS
               RECORD KEY IS EMP-ID.
       DATA DIVISION.
       FILE SECTION.
       FD  EMP-FILE.
       01  EMP-RECORD.
           05  EMP-ID    PIC 9(5).
           05  EMP-NAME  PIC X(20).
       WORKING-STORAGE SECTION.
       01  WS-STATUS     PIC XX.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
      * Error: Trying to READ from file opened OUTPUT only
           OPEN OUTPUT EMP-FILE
           READ EMP-FILE
               INVALID KEY
                   DISPLAY "KEY ERROR"
           END-READ
           CLOSE EMP-FILE.
           STOP RUN.
