
#  Wiki: https://developers.google.com/drive/api/quickstart/python

from flask import Flask, request, redirect, session, url_for, send_file
from google_auth_oauthlib.flow import Flow
from google.oauth2.credentials import Credentials
from googleapiclient.discovery import build
from googleapiclient.http import MediaFileUpload, MediaIoBaseDownload
import os
import pathlib
import io

app = Flask(__name__)
app.secret_key = 'REPLACE_WITH_A_SECRET_KEY'

# Directory where 'credentials.json' is located
CREDENTIALS_DIR = pathlib.Path(__file__).parent
os.environ['OAUTHLIB_INSECURE_TRANSPORT'] = '1'  # ONLY for development!

SCOPES = ['https://www.googleapis.com/auth/drive']

@app.route('/')
def index():
    # Check if credentials are already loaded
    if 'credentials' in session:
        creds = Credentials(**session['credentials'])
        if creds and creds.valid:
            return 'You are already authenticated. <br><a href="/upload_form">Upload File</a>'
        elif creds and creds.expired and creds.refresh_token:
            creds.refresh(Request())
            session['credentials'] = {
                'token': creds.token,
                'refresh_token': creds.refresh_token,
                'token_uri': creds.token_uri,
                'client_id': creds.client_id,
                'client_secret': creds.client_secret,
                'scopes': creds.scopes
            }
            return 'Credentials refreshed.'
        return 'Credentials are expired.'

    return '<a href="/authorize">Authorize</a>'

@app.route('/authorize')
def authorize():
    flow = Flow.from_client_secrets_file(
        str(CREDENTIALS_DIR / 'credentials.json'),
        scopes=SCOPES,
        redirect_uri=url_for('oauth2callback', _external=True)
    )
    authorization_url, state = flow.authorization_url(
        access_type='offline',
        include_granted_scopes='true'
    )
    session['state'] = state
    return redirect(authorization_url)

@app.route('/oauth2callback')
def oauth2callback():
    state = session['state']
    flow = Flow.from_client_secrets_file(
        str(CREDENTIALS_DIR / 'credentials.json'),
        scopes=SCOPES,
        state=state,
        redirect_uri=url_for('oauth2callback', _external=True)
    )
    flow.fetch_token(authorization_response=request.url)

    credentials = flow.credentials
    session['credentials'] = {
        'token': credentials.token,
        'refresh_token': credentials.refresh_token,
        'token_uri': credentials.token_uri,
        'client_id': credentials.client_id,
        'client_secret': credentials.client_secret,
        'scopes': credentials.scopes
    }

    return redirect(url_for('index'))

@app.route('/upload_form')
def upload_form():
    return '''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Upload to Google Drive</title>
        </head>
        <body>
            <h1>Upload File to Google Drive</h1>
            <form action="/upload" method="post" enctype="multipart/form-data">
                <input type="file" name="file">
                <input type="submit" value="Upload">
            </form>
        </body>
        </html>
    '''

@app.route('/upload', methods=['POST'])
def upload_file():
    if 'credentials' not in session:
        return redirect(url_for('authorize'))

    file = request.files['file']
    if not file:
        return "No file provided", 400

    creds = Credentials(**session['credentials'])
    service = build('drive', 'v3', credentials=creds)

    file_metadata = {'name': file.filename}
    media = MediaFileUpload(file.stream, mimetype=file.content_type)
    file = service.files().create(body=file_metadata, media_body=media, fields='id').execute()

    return f'File {file.get("name")} ID: {file.get("id")} uploaded successfully.'

@app.route('/download/<file_id>')
def download_file(file_id):
    if 'credentials' not in session:
        return redirect(url_for('authorize'))

    creds = Credentials(**session['credentials'])
    service = build('drive', 'v3', credentials=creds)

    request = service.files().get_media(fileId=file_id)
    fh = io.BytesIO()
    downloader = MediaIoBaseDownload(fh, request)

    done = False
    while not done:
        status, done = downloader.next_chunk()
        print(f"Download {int(status.progress() * 100)}%.")

    fh.seek(0)
    return send_file(fh, as_attachment=True, attachment_filename=f'{file_id}.bin')

if __name__ == '__main__':
    app.run('localhost', 8080)
