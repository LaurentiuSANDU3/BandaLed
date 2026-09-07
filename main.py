import sounddevice as sd
import numpy as np
import torch
from transformers import pipeline
import requests
import time

ESP32_IP = "192.168.100.137"
CALE_MODEL = "openai/whisper-tiny"
DURATA_INREGISTRARE = 4
RATE = 16000

print("Se incarca modelul AI local...")
whisper_pipe = pipeline(
    "automatic-speech-recognition",
    model=CALE_MODEL,
    tokenizer=CALE_MODEL,
    feature_extractor=CALE_MODEL,
    device="cpu"
)

print("Se incarca filtrul de zgomot...")
vad_model, vad_utils = torch.hub.load(
    repo_or_dir='snakers4/silero-vad',
    model='silero_vad',
    force_reload=False
)
(get_speech_timestamps, , _, *) = vad_utils

def trimite_comanda_http(url):
    try:
        raspuns = requests.get(url, timeout=2)
        if raspuns.status_code == 200:
            print("Comanda trimisa cu succes catre ESP32.")
        else:
            print("ESP32 a raspuns cu o eroare:", raspuns.status_code)
    except requests.exceptions.RequestException:
        print("Nu ma pot conecta la ESP32.")

def asculta_si_comanda():
    print("\nAscult", DURATA_INREGISTRARE, "secunde...")
    
    audio_data = sd.rec(int(DURATA_INREGISTRARE * RATE), samplerate=RATE, channels=1, dtype='float32')
    sd.wait()
    
    audio_tensor = torch.from_numpy(audio_data.squeeze())
    speech_timestamps = get_speech_timestamps(audio_tensor, vad_model, sampling_rate=RATE, threshold=0.5)
    
    if len(speech_timestamps) > 0:
        print("Voce detectata! Se decodifica...")
        
        rezultat = whisper_pipe(audio_data.squeeze())
        text = rezultat["text"].strip().lower()
        
        print("  -> Ai spus:", text)
        
        url_baza = f"http://{ESP32_IP}/mode"
        
        if "pornește mod 1" in text or "porneste mod 1" in text or "mod unu" in text or "mod 1" in text:
            print("Schimb pe Mod 1 (Rosu).")
            trimite_comanda_http(f"{url_baza}?m=1")
            
        elif "pornește mod 2" in text or "porneste mod 2" in text or "mod doi" in text or "mod 2" in text:
            print("Schimb pe Mod 2 (Galben).")
            trimite_comanda_http(f"{url_baza}?m=2")
            
        elif "pornește mod 3" in text or "porneste mod 3" in text or "mod trei" in text or "mod 3" in text:
            print("Schimb pe Mod 3 (Verde).")
            trimite_comanda_http(f"{url_baza}?m=3")
            
        elif "pornește mod 4" in text or "porneste mod 4" in text or "mod patru" in text or "mod 4" in text:
            print("Schimb pe Mod 4 (Albastru).")
            trimite_comanda_http(f"{url_baza}?m=4")
            
        elif "pornește mod 5" in text or "porneste mod 5" in text or "mod cinci" in text or "mod 5" in text:
            print("Schimb pe Mod 5 (Alb).")
            trimite_comanda_http(f"{url_baza}?m=5")
            
        elif "stinge" in text or "oprește" in text or "opreste" in text:
            print("Sting toate LED-urile.")
            trimite_comanda_http(f"http://{ESP32_IP}/off")
            
        else:
            print("Comanda nerecunoscuta.")
    else:
        print("Liniste (zgomot ignorat).")

print("\nSISTEM ACTIVAT")
try:
    while True:
        asculta_si_comanda()
except KeyboardInterrupt:
    print("\nSistem oprit de la tastatura.")