#!/usr/bin/env python3
"""Gera o QR do "cafezinho" (PIX estático, padrão BR Code do Banco Central) da aba Sobre.

Uso: python tools/pix-qr.py  (requer o pacote segno apenas para gerar a imagem)
Saída: assets/pix-cafezinho.png e assets/pix-cafezinho.txt (código "copia e cola").
"""
import pathlib
import sys

KEY = "eritongrnascimento@gmail.com"
NAME = "ERITON DO NASCIMENTO"  # campo 59: até 25 caracteres, sem acentos
CITY = "RECIFE"  # campo 60: até 15 caracteres
DESCRIPTION = "Cafezinho GPU-PRO"


def field(tag: str, value: str) -> str:
    if len(value) > 99:
        raise ValueError(f"campo {tag} longo demais")
    return f"{tag}{len(value):02d}{value}"


def crc16_ccitt(data: str) -> str:
    """CRC16-CCITT-FALSE (polinômio 0x1021, valor inicial 0xFFFF), exigido no campo 63."""
    crc = 0xFFFF
    for byte in data.encode("utf-8"):
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return f"{crc:04X}"


def payload() -> str:
    account = field("00", "br.gov.bcb.pix") + field("01", KEY) + field("02", DESCRIPTION)
    body = (
        field("00", "01")
        + field("01", "11")  # QR estático, reutilizável
        + field("26", account)
        + field("52", "0000")
        + field("53", "986")  # BRL
        + field("58", "BR")
        + field("59", NAME[:25])
        + field("60", CITY[:15])
        + field("62", field("05", "***"))
        + "6304"
    )
    return body + crc16_ccitt(body)


def main() -> int:
    assert crc16_ccitt("123456789") == "29B1", "CRC fora do padrão CCITT-FALSE"
    code = payload()
    root = pathlib.Path(__file__).resolve().parent.parent
    (root / "assets" / "pix-cafezinho.txt").write_text(code + "\n", encoding="ascii")
    import segno

    segno.make(code, error="m", micro=False).save(
        root / "assets" / "pix-cafezinho.png", scale=8, border=3, dark="#0b0b0f", light="#ffffff"
    )
    print(code)
    return 0


if __name__ == "__main__":
    sys.exit(main())
