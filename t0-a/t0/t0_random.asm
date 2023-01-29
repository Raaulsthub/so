; T0 DE SISTEMAS OPERACIONAIS
; Programa que imprime 10 numeros randomicos em ordem crescente

contador VALOR 10   ; int contador = 10
    CARGI 0         ; a = 0
    MVAX            ; x = 0
for
    LE 3            ; a = leitura random
    ESCR 0          ; print(a)
    INCX            ; x += 1
    MVXA            ; a = x
    SUB contador    ; a -= contador
    DESVNZ for      ; se a != 0 (a != contador) desvia para for
    PARA

