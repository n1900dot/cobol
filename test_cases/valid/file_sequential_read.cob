       IDENTIFICATION DIVISION.
       PROGRAM-ID. FILE-SEQ-READ.
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
           OPEN INPUT EMP-FILE
           IF WS-FILE-STATUS NOT = "00"
               DISPLAY "ERROR OPENING FILE: " WS-FILE-STATUS
               STOP RUN
           END-IF
           
           PERFORM UNTIL END-OF-FILE
               READ EMP-FILE
                   AT END
                       SET END-OF-FILE TO TRUE
                   NOT AT END
                       DISPLAY "ID: " EMP-ID 
                               " NAME: " EMP-NAME
                               " SALARY: " EMP-SALARY
               END-READ
           END-PERFORM
           
           CLOSE EMP-FILE
           DISPLAY "FILE READ COMPLETE".
           STOP RUN.
