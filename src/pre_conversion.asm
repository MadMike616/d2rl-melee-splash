; RuffnecKk: process-lifetime, call-site-only adapter for the captured Loader
; Fill path. Preserve all volatile state and flags, then replay the exact
; displaced MOVZX. Standard x64 unwind metadata describes both stack changes.
OPTION CASEMAP:NONE
EXTERN MeleeSplashCaptureBeforeConversion:PROC
PUBLIC MeleeSplashPreConversionRelay
PUBLIC MeleeSplashRelayAfterPushRbp
PUBLIC MeleeSplashRelayAfterPushFlags
PUBLIC MeleeSplashRelayAfterAllocate
PUBLIC MeleeSplashRelayBody
PUBLIC MeleeSplashRelayBeforeFlagsRestore
PUBLIC MeleeSplashRelayAfterFlagsPush
PUBLIC MeleeSplashRelayBeforeMovzx
PUBLIC MeleeSplashRelayEpilog
PUBLIC MeleeSplashRelayPopRbp
PUBLIC MeleeSplashRelayRet
.code
MeleeSplashPreConversionRelay PROC FRAME
    push rbp
    .pushreg rbp
MeleeSplashRelayAfterPushRbp LABEL BYTE
    pushfq
    .allocstack 8
MeleeSplashRelayAfterPushFlags LABEL BYTE
    sub rsp, 0C8h
    .allocstack 0C8h
MeleeSplashRelayAfterAllocate LABEL BYTE
    lea rbp, [rsp+80h]
    .setframe rbp, 80h
    .endprolog
MeleeSplashRelayBody LABEL BYTE
    mov [rsp+80h], rax
    mov [rsp+88h], rcx
    mov [rsp+90h], rdx
    mov [rsp+98h], r8
    mov [rsp+0A0h], r9
    mov [rsp+0A8h], r10
    mov [rsp+0B0h], r11
    movdqu xmmword ptr [rsp+20h], xmm0
    movdqu xmmword ptr [rsp+30h], xmm1
    movdqu xmmword ptr [rsp+40h], xmm2
    movdqu xmmword ptr [rsp+50h], xmm3
    movdqu xmmword ptr [rsp+60h], xmm4
    movdqu xmmword ptr [rsp+70h], xmm5
    call MeleeSplashCaptureBeforeConversion
    movdqu xmm0, xmmword ptr [rsp+20h]
    movdqu xmm1, xmmword ptr [rsp+30h]
    movdqu xmm2, xmmword ptr [rsp+40h]
    movdqu xmm3, xmmword ptr [rsp+50h]
    movdqu xmm4, xmmword ptr [rsp+60h]
    movdqu xmm5, xmmword ptr [rsp+70h]
    mov rax, [rsp+80h]
    mov rcx, [rsp+88h]
    mov rdx, [rsp+90h]
    mov r8, [rsp+98h]
    mov r9, [rsp+0A0h]
    mov r10, [rsp+0A8h]
    mov r11, [rsp+0B0h]
MeleeSplashRelayBeforeFlagsRestore LABEL BYTE
    push qword ptr [rsp+0C8h]
MeleeSplashRelayAfterFlagsPush LABEL BYTE
    popfq
MeleeSplashRelayBeforeMovzx LABEL BYTE
    movzx r10d, byte ptr [rdi+149h]
MeleeSplashRelayEpilog LABEL BYTE
    lea rsp, [rbp+50h]
MeleeSplashRelayPopRbp LABEL BYTE
    pop rbp
MeleeSplashRelayRet LABEL BYTE
    ret
MeleeSplashPreConversionRelay ENDP
END
