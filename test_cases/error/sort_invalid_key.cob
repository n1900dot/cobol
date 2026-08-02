       IDENTIFICATION DIVISION.
       PROGRAM-ID. INVALID-SORT-KEY.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT EMP-FILE ASSIGN TO "EMP.DAT"
               ORGANIZATION IS SEQUENTIAL.
       DATA DIVISION.
       FILE SECTION.
       FD  EMP-FILE.
       01  EMP-RECORD.
           05  EMP-ID        PIC 9(5).
           05  EMP-NAME      PIC X(20).
       WORKING-STORAGE SECTION.
       01  WS-SORT-WORK.
           05  WS-ID         PIC 9(5).
           05  WS-NAME       PIC X(20).
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
      * Error: Sorting on a field not in the sort record structure
           SORT WS-SORT-WORK
               ON ASCENDING KEY EMP-ID
               INPUT PROCEDURE IS LOAD-DATA
           END-SORT.
           STOP RUN.
       
       LOAD-DATA.
           OPEN INPUT EMP-FILE
           PERFORM UNTIL FALSE
               READ EMP-FILE
                   AT END EXIT PERFORM
                   RELEASE WS-SORT-WORK
               END-READ
           END-PERFORM
           CLOSE EMP-FILE.
