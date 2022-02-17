Mögliche Messmethoden:
 
* v1.1: Iterative Messungen mit festgelegter Frequenz und Ein- und Ausgangszählern um die zusammengehörigen Signale in Verbindung zu bringen.

Vorteile:
- Laufzeit (s) = Anzahl der Messungen / Messfrequenz (Hz) + Durchschnittliche Latenz des Prüflings (s)
- Die aus der Messfrequenz resultierende Phasenlänge des Signals kann kleiner als die Latenz des Prüflings sein

Nachteile: 
- Es darf kein Rauschen in Form von zusätzlichen oder fehlenden Signalen auftreten, da sonst die Ein- und Ausgangssignale falsch gemappt werden und somit verfälschte Messungen entstehen. 


* v1.2: Iterative Messungen mit einer niedrigen Startfrequenz, die sich im Laufe des Messvorgangs an die aktuell gemessene maximale Latenz des Prüflings annähert.

Vorteile:
- Rauschen in Form von nicht registrierten Signalen verfälscht die Messungen nicht, sondern überspringt diese durch erneutes zuweisen des Start-Zeitstempels.

Nachteile:
- Rauschen in Form von zusätzlich registrierten Signalen kann die Messungen verfälschen.
- Laufzeit (s) = Anzahl der Messungen * Maximale Latenz des Prüflings (s)


* v2: Rekursive Messungen wobei nach dem Startsignal, bei Signaleingang das jeweils nächste Signal ausgegeben wird. Ein Timer im Signalausgang dient dazu die Messung durch erneutes schicken eines Ausgangssignals bei Signalverlust aufrecht zu erhalten.

Vorteile:
- Rauschen in Form von nicht registrierten Signalen verfälscht die Messungen nicht, sondern überspringt diese durch erneutes senden eines Ausgangssignals ab einer gewissen Zeitgrenze.

Nachteile:
- Diese Zeitgrenze bestimmt die maximal messbare Latenz des Systems.
- Rauschen in Form von zusätzlich registrierten Signalen kann die Messungen verfälschen.
- Laufzeit (s) = Anzahl der Messungen * Durchschnittliche Latenz des Prüflings (s)


* v3: Signale in Form von Bitsequenzen senden. Durch die Kodierung können die Eingangssignale zu den richtigen Ausgangssignalen verlinkt werden.

Vorteile:
- Ein größeres Ausmaß an Signalrauschen kann durch Paritätsbits festgestellt werden.
- Laufzeit (s) = Anzahl der Messungen * Durchschnittliche Anzahl an Bits pro Messung * Bitlänge (s) + Latenz des Prüflings (s)
- Die aus der Bitlänge resultierende Phasenlänge des Signals kann kleiner als die Latenz des Prüflings sein

Nachteile:
- Audio-Ausgabegeräte, die die Phase des Signals verkleinern können nicht gemessen werden, da sie die Bitsequenz verändern.