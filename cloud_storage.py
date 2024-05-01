from flask import Flask, request, redirect, url_for, session, send_file
from google_auth_oauthlib.flow import Flow
from google.oauth2.credentials import Credentials
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload, MediaIoBaseDownload
import os
import pathlib
import io
from tempfile import NamedTemporaryFile
from functools import wraps

app = Flask(__name__)
app.secret_key = "REPLACE_WITH_A_SECRET_KEY"

# Directory where 'credentials.json' is located
CREDENTIALS_DIR = pathlib.Path(__file__).parent
os.environ["OAUTHLIB_INSECURE_TRANSPORT"] = "1"  # ONLY for development!
SCOPES = ["https://www.googleapis.com/auth/drive"]

def token_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        # Check if session contains credentials
        if 'credentials' in session and session['credentials'].get('token'):
            creds = Credentials(**session['credentials'])
            if not creds.valid:
                if creds.expired and creds.refresh_token:
                    creds.refresh(Request())
                    session['credentials'] = {
                        'token': creds.token,
                        'refresh_token': creds.refresh_token,
                        'token_uri': creds.token_uri,
                        'client_id': creds.client_id,
                        'client_secret': creds.client_secret,
                        'scopes': creds.scopes
                    }
                else:
                    return 404, 404
        else:
            # Try to get the token from the Authorization header
            auth_header = request.headers.get('Authorization')
            refresh_token = request.headers.get('X-Refresh-Token')
            token_uri = request.headers.get('X-Token-Uri')
            client_id = request.headers.get('X-Client-Id')
            client_secret = request.headers.get('X-Client-Secret')

            if auth_header and auth_header.startswith('Bearer '):
                token = auth_header.split(" ")[1]
                creds = Credentials(token=token, refresh_token=refresh_token, token_uri=token_uri, client_id=client_id, client_secret=client_secret)
            else:
                print("No credentials found")
                return 404, 404

        # Attach creds to the current request context
        request.creds = creds
        return f(*args, **kwargs)
    return decorated_function

@app.route("/")
def index():
    if "credentials" not in session or not session["credentials"].get('token'):
        return redirect(url_for("authorize"))
    return ('You are already authenticated. <br>'
            '<a href="/upload_to_folder_form">Upload File</a> <br>'
            '<a href="/download_form">Download File</a>')

@app.route("/authorize")
def authorize():
    flow = Flow.from_client_secrets_file(
        str(CREDENTIALS_DIR / "credentials.json"),
        scopes=SCOPES,
        redirect_uri=url_for("oauth2callback", _external=True))
    authorization_url, state = flow.authorization_url(
        access_type="offline", include_granted_scopes="true")
    session['state'] = state
    return redirect(authorization_url)

@app.route("/oauth2callback")
def oauth2callback():
    state = session.get('state')
    flow = Flow.from_client_secrets_file(
        str(CREDENTIALS_DIR / "credentials.json"),
        scopes=SCOPES,
        state=state,
        redirect_uri=url_for("oauth2callback", _external=True))
    flow.fetch_token(authorization_response=request.url)
    credentials = flow.credentials
    session["credentials"] = {
        "token": credentials.token,
        "refresh_token": credentials.refresh_token,
        "token_uri": credentials.token_uri,
        "client_id": credentials.client_id,
        "client_secret": credentials.client_secret,
        "scopes": credentials.scopes
    }

    # Write the tokens to a file securely which will be used by c client
    with open("tokens.txt", "w") as token_file:
        token_file.write(f"access_token={credentials.token}\n")
        token_file.write(f"refresh_token={credentials.refresh_token}\n")
        token_file.write(f"token_uri={credentials.token_uri}\n")
        token_file.write(f"client_id={credentials.client_id}\n")
        token_file.write(f"client_secret={credentials.client_secret}\n")
        token_file.write(f"scopes={credentials.scopes}\n")

    return redirect(url_for("index"))

@app.route("/upload_to_folder_form")
def upload_to_folder_form():
    return """
        <!DOCTYPE html>
        <html>
        <head>
            <title>Upload File to Google Drive Folder</title>
        </head>
        <body>
            <h1>Upload File to Specific Folder on Google Drive</h1>
            <form action="/upload_by_folder_name" method="post" enctype="multipart/form-data">
                Folder Name: <input type="text" name="folder_name" required><br>
                File Name: <input type="text" name="file_name" required><br>
                Select file: <input type="file" name="file" required><br>
                <input type="submit" value="Upload">
            </form>
        </body>
        </html>
    """


@app.route("/upload_by_folder_name", methods=["POST"])
@token_required
def upload_by_folder_name():
    try:
        folder_name = request.form['folder_name']
        file_name = request.form['file_name']
        
        file = request.files["file"]
        if not file:
            return "404", 404

        service = build("drive", "v3", credentials=request.creds)

        # Search for the folder by name and get the folder ID
        query = f"mimeType='application/vnd.google-apps.folder' and name='{folder_name}' and trashed=false"
        folder_results = service.files().list(q=query, spaces="drive", fields="files(id)").execute()
        folders = folder_results.get("files", [])

        if not folders:
            return "404", 404

        folder_id = folders[0]["id"]

        with NamedTemporaryFile(delete=False) as tmp:
            file.save(tmp.name)
            tmp.close()
            file_metadata = {"name": file_name, "parents": [folder_id]}
            media = MediaFileUpload(tmp.name, mimetype=file.content_type)

            # Check if file exists
            query = f"'{folder_id}' in parents and name='{file_name}' and trashed=false"
            existing_files = service.files().list(q=query).execute().get("files", [])

            if existing_files:
                file_id = existing_files[0]["id"]
                file_metadata = {"name": file_name}
                updated_file = service.files().update(fileId=file_id, body=file_metadata, media_body=media).execute()
                response_message = f'File {updated_file.get("name")} updated successfully.'
            else:
                created_file = service.files().create(body=file_metadata, media_body=media, fields="id").execute()
                response_message = f'File {created_file.get("name")} created successfully.'
            
            # os.remove(tmp.name)

        return response_message
    except Exception as e:
        print(e)
        return "404", 404


@app.route("/download_by_folder_name", methods=["GET"])
@token_required
def download_by_folder_name():
    try:
        folder_name = request.args.get('folder_name')
        filename = request.args.get('filename')
        if not folder_name or not filename:
            return "404", 404
        
        service = build("drive", "v3", credentials=request.creds)

        # Search for the folder by name and get the folder ID
        folder_query = f"mimeType='application/vnd.google-apps.folder' and name='{folder_name}' and trashed=false"
        folder_results = service.files().list(q=folder_query, spaces="drive", fields="files(id)").execute()
        folders = folder_results.get("files", [])
        if not folders:
            return "404", 404

        folder_id = folders[0]["id"]

        # Search for the file by name within the resolved folder
        file_query = f"'{folder_id}' in parents and name='{filename}' and trashed=false"
        file_results = service.files().list(q=file_query, spaces="drive", fields="files(id, name)").execute()
        files = file_results.get("files", [])
        print("[]",files)
        if not files:
            return "404", 404

        file_id = files[0]["id"]
        file_request = service.files().get_media(fileId=file_id)
        fh = io.BytesIO()
        downloader = MediaIoBaseDownload(fh, file_request)

        done = False
        while not done:
            status, done = downloader.next_chunk()

        fh.seek(0)
        print(fh)
        return send_file(fh, as_attachment=True, download_name = filename)
    except Exception as e:
        print(e)
        return "404", 404


@app.route("/download_form")
def download_form():
    return """
        <!DOCTYPE html>
        <html>
        <head>
            <title>Download File from Google Drive</title>
        </head>
        <body>
            <h1>Download File from Specific Folder on Google Drive</h1>
            <form action="/download_by_folder_name" method="get">
                Folder Name: <input type="text" name="folder_name" required><br>
                File Name: <input type="text" name="filename" required><br>
                <input type="submit" value="Download">
            </form>
        </body>
        </html>
    """


if __name__ == "__main__":
    app.run("localhost", 8080, debug=True)