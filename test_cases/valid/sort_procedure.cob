       IDENTIFICATION DIVISION.
       PROGRAM-ID. SORT-DEMO.
       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT EMP-INPUT ASSIGN TO "EMP-IN.DAT"
               ORGANIZATION IS SEQUENTIAL.
           SELECT EMP-SORTED ASSIGN TO "EMP-OUT.DAT"
               ORGANIZATION IS SEQUENTIAL.
       DATA DIVISION.
       FILE SECTION.
       FD  EMP-INPUT.
       01  EMP-IN-RECORD.
           05  IN-EMP-ID       PIC 9(5).
           05  IN-EMP-NAME     PIC X(20).
           05  IN-EMP-SALARY   PIC 9(7)V99.
       FD  EMP-SORTED.
       01  EMP-OUT-RECORD.
           05  OUT-EMP-ID      PIC 9(5).
           05  OUT-EMP-NAME    PIC X(20).
           05  OUT-EMP-SALARY  PIC 9(7)V99.
       WORKING-STORAGE SECTION.
       01  WS-SORT-RECORD.
           05  SORT-EMP-ID     PIC 9(5).
           05  SORT-EMP-NAME   PIC X(20).
           05  SORT-EMP-SALARY PIC 9(7)V99.
       PROCEDURE DIVISION.
       MAIN-PROCEDURE.
           SORT WS-SORT-RECORD
               ON ASCENDING KEY SORT-EMP-SALARY
               INPUT PROCEDURE IS LOAD-INPUT
               OUTPUT PROCEDURE IS WRITE-OUTPUT.
           
           DISPLAY "SORT COMPLETE".
           STOP RUN.
       
       LOAD-INPUT.
           OPEN INPUT EMP-INPUT
           PERFORM UNTIL FALSE
               READ EMP-INPUT
                   AT END EXIT PERFORM
                   NOT AT END
                       MOVE IN-EMP-ID TO SORT-EMP-ID
                       MOVE IN-EMP-NAME TO SORT-EMP-NAME
                       MOVE IN-EMP-SALARY TO SORT-EMP-SALARY
                       RELEASE WS-SORT-RECORD
               END-READ
           END-PERFORM
           CLOSE EMP-INPUT.
       
       WRITE-OUTPUT.
           OPEN OUTPUT EMP-SORTED
           PERFORM UNTIL FALSE
               RETURN WS-SORT-RECORD
                   AT END EXIT PERFORM
                   NOT AT END
                       MOVE SORT-EMP-ID TO OUT-EMP-ID
                       MOVE SORT-EMP-NAME TO OUT-EMP-NAME
                       MOVE SORT-EMP-SALARY TO OUT-EMP-SALARY
                       WRITE EMP-OUT-RECORD
               END-RETURN
           END-PERFORM
           CLOSE EMP-SORTED.
