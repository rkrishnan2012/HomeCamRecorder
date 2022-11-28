import socketserver

class MyTCPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.data = self.request.recv(1024).strip()
        print("{} wrote:".format(self.client_address[0]))
        print(self.data)
        # # just send back the same data, but upper-cased
        example_output = b"\xcc\xdd\xee\xff\x63\x27\x00\x00\xe5\x12\x69\x00\x36\x00\x00\x00\x00\x00\x00\x00\x00\x30\x56\x63\x34\x6f\x88\xb0\x76\x37\xba\x75\xe5\x14\x50\x9d\x04\x4d\x57\xe1\xc6\xdd\x18\x48\x0c\x42\x09\xf4\x9e\xc3\x5b\x47\xca\x36"
        self.request.sendall(example_output)

if __name__ == "__main__":
    HOST, PORT = "0.0.0.0", 19000

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer((HOST, PORT), MyTCPHandler) as server:
        server.allow_reuse_address = True
        server.serve_forever()