import os
import subprocess
from flask import Flask, render_template, request, jsonify, send_file

app = Flask(__name__)

UPLOAD_FOLDER = os.path.join(os.getcwd(), "storage")
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# Select executable based on platform
EXECUTABLE = "FileArchiver.exe" if os.name == "nt" else "./FileArchiver"

def get_readable_size(size_bytes):
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} TB"

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/compress", methods=["POST"])
def compress_file_endpoint():
    if "file" not in request.files:
        return jsonify({"error": "No file uploaded"}), 400

    uploaded_file = request.files["file"]
    if uploaded_file.filename == "":
        return jsonify({"error": "No file selected"}), 400

    filename = uploaded_file.filename
    input_path = os.path.join(UPLOAD_FOLDER, filename)
    archive_path = os.path.join(UPLOAD_FOLDER, f"{filename}.huff")

    uploaded_file.save(input_path)

    # Invoke C backend: FileArchiver.exe -c <input> <archive>
    cmd = [EXECUTABLE, "-c", input_path, archive_path]
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0 or not os.path.exists(archive_path):
        return jsonify({"error": f"C engine error: {result.stderr or 'Compression failed'}"}), 500

    orig_size = os.path.getsize(input_path)
    comp_size = os.path.getsize(archive_path)
    ratio = (comp_size / orig_size) if orig_size > 0 else 0
    saved = (1.0 - ratio) * 100.0

    return jsonify({
        "success": True,
        "filename": filename,
        "archive_name": f"{filename}.huff",
        "original_size": get_readable_size(orig_size),
        "compressed_size": get_readable_size(comp_size),
        "ratio": f"{ratio:.4f}",
        "space_saved": f"{saved:.2f}%",
        "download_url": f"/download/{os.path.basename(archive_path)}"
    })

@app.route("/api/extract", methods=["POST"])
def extract_file_endpoint():
    if "file" not in request.files:
        return jsonify({"error": "No .huff archive uploaded"}), 400

    uploaded_file = request.files["file"]
    archive_name = uploaded_file.filename

    if not archive_name.endswith(".huff"):
        return jsonify({"error": "File must have a .huff extension"}), 400

    archive_path = os.path.join(UPLOAD_FOLDER, archive_name)
    uploaded_file.save(archive_path)

    # Let C binary automatically restore original filename prefixed with extracted_
    cmd = [EXECUTABLE, "-x", archive_path]
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=UPLOAD_FOLDER)

    if result.returncode != 0:
        return jsonify({"error": f"Extraction failed: {result.stderr}"}), 500

    # Determine extracted output name (format: extracted_<original_name>)
    base_target = archive_name[:-5]
    extracted_filename = f"extracted_{base_target}"
    extracted_path = os.path.join(UPLOAD_FOLDER, extracted_filename)

    # If the file exists, return the download link
    if os.path.exists(extracted_path):
        return jsonify({
            "success": True,
            "filename": extracted_filename,
            "download_url": f"/download/{extracted_filename}"
        })

    # Search folder for any newly extracted file
    return jsonify({"success": True, "message": "Extracted successfully into project directory."})

@app.route("/download/<path:filename>")
def download_file(filename):
    file_path = os.path.join(UPLOAD_FOLDER, filename)
    return send_file(file_path, as_attachment=True)

if __name__ == "__main__":
    print("Starting Huffman Web GUI at http://127.0.0.1:5000")
    app.run(debug=True, port=5000)