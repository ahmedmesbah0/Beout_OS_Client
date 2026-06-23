#!/usr/bin/env python3
import http.server
import ssl
import json
import base64
import os
import subprocess
import urllib.parse

# This is a Mock Licensing Server for Beout_OS
# It signs the provided Machine ID with a generated private key.

HOST = '0.0.0.0'
PORT = 8443
CERT_FILE = 'server.crt'
KEY_FILE = 'server.key'
SIGNING_KEY = 'signing.key'

def generate_certs():
    if not os.path.exists(CERT_FILE):
        print("Generating self-signed SSL certificate...")
        subprocess.run([
            'openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-keyout', KEY_FILE, '-out', CERT_FILE,
            '-days', '365', '-nodes', '-subj', '/CN=localhost'
        ], check=True)

def generate_signing_key():
    if not os.path.exists(SIGNING_KEY):
        print("Generating Ed25519 signing key pair...")
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'ED25519', '-out', SIGNING_KEY], check=True)
        subprocess.run(['openssl', 'pkey', '-in', SIGNING_KEY, '-pubout', '-out', 'signing.pub'], check=True)

class LicensingRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path in ('/api/v1/activate', '/api/license/activate'):
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            
            try:
                data = json.loads(post_data.decode('utf-8'))
                machine_id = data.get('machine_id')
                
                if not machine_id:
                    self.send_error(400, "Missing machine_id")
                    return
                
                # Sign the machine_id (Ed25519 requires openssl pkeyutl with a file input)
                import tempfile
                with tempfile.NamedTemporaryFile(delete=False) as temp_in:
                    temp_in.write(machine_id.encode('utf-8'))
                    temp_in_path = temp_in.name
                
                try:
                    process = subprocess.run(
                        ['openssl', 'pkeyutl', '-sign', '-inkey', SIGNING_KEY, '-rawin', '-in', temp_in_path],
                        capture_output=True,
                        check=True
                    )
                    signature = base64.b64encode(process.stdout).decode('utf-8')
                finally:
                    if os.path.exists(temp_in_path):
                        os.unlink(temp_in_path)
                
                response = {
                    'status': 'success',
                    'token': signature,
                    'activation_token': signature
                }
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(response).encode('utf-8'))
                
            except Exception as e:
                self.send_error(500, f"Internal Server Error: {str(e)}")
        else:
            self.send_error(404, "Not Found")

def main():
    generate_certs()
    generate_signing_key()
    
    server = http.server.HTTPServer((HOST, PORT), LicensingRequestHandler)
    
    # Wrap with SSL
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=CERT_FILE, keyfile=KEY_FILE)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    
    print(f"Mock Licensing Server running on https://{HOST}:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()

if __name__ == '__main__':
    main()
