# Aliens versus Predator 1 & 2 para Xbox 360 (`avp360`)

Port para Xbox 360 (RGH/JTAG) de **Aliens versus Predator (1999, Gold Edition)** e **Aliens versus Predator 2
(2001)**, feito por **Matherzin** com o [xsdk](https://github.com/xenosworks/xsdk). Um menu ("The Double Feature")
escolhe o jogo; cada jogo roda no seu próprio `.xex` e volta ao menu ao sair.

| `.xex` | O que é |
|---|---|
| `AvP360.xex` / `default.xex` | o menu |
| `avp1.xex` | AvP 1: o motor da Rebellion via [NakedAVP](https://github.com/atsb/NakedAVP), com renderizador Xenos |
| `avp2.xex` | AvP 2: o LithTech 2.2 do [AVP2 Reconstructed](https://github.com/Kizaing/NOLF-Reconstructed) + o código do jogo, com renderizador, controle e som do Xbox |

**Estado:** em desenvolvimento, ainda não testado de ponta a ponta no console. Detalhes em
[docs/STATUS.md](docs/STATUS.md).

## No console

Na pasta do `default.xex`, os arquivos dos **seus próprios** jogos:

```text
default.xex  AvP360.xex  avp1.xex  avp2.xex
avp1\    AvP 1 Gold (avp_huds.rif, fastfile\, language.txt...)
avp2\    AvP 2 (AVP2.REZ, AVP2L.REZ, SOUNDS.REZ, ..., autoexec.cfg, ltmsg.dll, cres.dll)
```

Nenhum dado dos jogos está neste repositório nem nos `.xex`.

## Compilar

No WSL (Debian/Ubuntu) com o xsdk, o `xsdk-gcc` e o SDL-xenon em `$HOME`, e o repositório num disco do Windows
(`/mnt/c/...`: o código do AvP2 mistura maiúsculas e minúsculas nos nomes):

```bash
XSDK_HOME=$HOME ./build.sh
```

O `build.sh` baixa os repositórios públicos em commits fixos e aplica os patches deste repositório:

| Parte | Origem | Patch |
|---|---|---|
| AvP 1 | NakedAVP (`platform/xbox360/avp1/patches/base-commit.txt`) | `avp1.patch` + `tools/endian_reads.py` |
| Motor do AvP 2 | AVP2 Reconstructed (`platform/xbox360/avp2/patches/avp2-reconstructed.base`) | `avp2-reconstructed.patch` |
| lithshared | [nolfsrc](https://github.com/nolffan/nolfsrc) (`nolfsrc.base`) | `nolfsrc-lithshared.patch` |
| Jogo e SDK do AvP 2 | **AvP2 Source Code v1.0.9.6** (Monolith/Fox, 2002): a licença não permite redistribuir, então cada um usa a sua cópia, na pasta `proj/` (`AVP2/` e `LT2/`) ou em `AVP2_SOURCE` | `avp2-source.patch` (em preparo) |

Sem o código-fonte do AvP 2, o `build.sh` compila só o menu e o AvP 1.

## Créditos e licenças

- **Aliens versus Predator** © 1999-2000 Rebellion. O código-fonte é usado de acordo com a licença dele: sem fins
  comerciais e com crédito à Rebellion como criadora e dona.
- **Aliens versus Predator 2** © 2001 Monolith Productions / Fox Interactive; LithTech © Monolith.
- [NakedAVP](https://github.com/atsb/NakedAVP), [AVP2 Reconstructed](https://github.com/Kizaing/NOLF-Reconstructed)
  e [nolfsrc](https://github.com/nolffan/nolfsrc) pelos seus autores.
- [xsdk](https://github.com/xenosworks/xsdk).

Projeto de fãs, sem fins comerciais, sem ligação com a Rebellion, a Monolith, a Fox, a Disney ou a Microsoft.
