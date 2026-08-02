       IDENTIFICATION DIVISION.
       PROGRAM-ID. FILE-NOT-DECLARED.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
      * File selected but FD not defined - Error
           SELECT EMP-FILE ASSIGN TO "EMP.DAT"
               ORGANIZATION IS SEQUENTIAL.
       DATA DIVISION.
      * Missing FD section for EMP-FILE
       WORKING-STORAGE SECTION.
       01  WS-DUMMY PIC 9.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           OPEN INPUT EMP-FILE
           READ EMP-FILE
           CLOSE EMP-FILE
           STOP RUN.
