from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.backends import default_backend
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID
import datetime
import ipaddress

# --- CONFIGURATION ---
ESP_IP = "192.168.1.x" 
# ---------------------

print(f"Generating certificate for IP: {ESP_IP}...")

# 1. Generate Private Key
key = rsa.generate_private_key(
    public_exponent=65537,
    key_size=2048,
    backend=default_backend()
)
key_pem = key.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.TraditionalOpenSSL,
    encryption_algorithm=serialization.NoEncryption()
)

# 2. Build Certificate
subject = issuer = x509.Name([
    x509.NameAttribute(NameOID.COMMON_NAME, u"ESP32-WebSocket"),
    x509.NameAttribute(NameOID.ORGANIZATION_NAME, u"NuSock"),
])

builder = x509.CertificateBuilder()
builder = builder.subject_name(subject)
builder = builder.issuer_name(issuer)
builder = builder.public_key(key.public_key())
builder = builder.serial_number(x509.random_serial_number())
builder = builder.not_valid_before(datetime.datetime.utcnow())
builder = builder.not_valid_after(datetime.datetime.utcnow() + datetime.timedelta(days=365))

# 3. Add Extensions (CRITICAL FOR CHROME/EDGE)

# A. Basic Constraints (Self-signed as CA)
builder = builder.add_extension(
    x509.BasicConstraints(ca=True, path_length=None), 
    critical=True,
)

# B. Subject Alternative Name (SAN) - Matches the IP
builder = builder.add_extension(
    x509.SubjectAlternativeName([
        x509.IPAddress(ipaddress.IPv4Address(ESP_IP))
    ]),
    critical=False
)

# C. Key Usage (Digital Signature, Key Encipherment)
builder = builder.add_extension(
    x509.KeyUsage(
        digital_signature=True,
        content_commitment=False,
        key_encipherment=True,
        data_encipherment=False,
        key_agreement=False,
        key_cert_sign=True,  # Needed because we claimed CA=True
        crl_sign=True,
        encipher_only=False,
        decipher_only=False
    ),
    critical=True
)

# D. Extended Key Usage (Server Authentication) - REQUIRED by Chrome
builder = builder.add_extension(
    x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]),
    critical=False
)

# 4. Sign and Output
cert = builder.sign(key, hashes.SHA256(), default_backend())
cert_pem = cert.public_bytes(serialization.Encoding.PEM)

print("\n--- COPY CONTENT BELOW TO main.cpp ---")
print("const char* server_cert = R\"EOF(" + cert_pem.decode('utf-8') + ")EOF\";")
print("\nconst char* server_key = R\"EOF(" + key_pem.decode('utf-8') + ")EOF\";")