"""Deliver a single IPA through an unpublished GitHub draft, never a public release."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
from urllib.request import Request,urlopen

def gh(*args):
 return subprocess.check_output(['gh',*map(str,args)],text=True)
def api(path):return json.loads(gh('api',path))
def private_release(release,sha):
 if release.get('draft') is not True or release.get('target_commitish')!=sha:
  raise RuntimeError('Refusing delivery: release is published or belongs to another source revision')
def create_private_draft(base,payload):
 # Send a complete JSON body directly. Some hosted gh clients fail while
 # decoding the draft-creation response before returning any response body.
 request=Request('https://api.github.com/'+base,
     data=json.dumps(payload).encode('utf-8'),method='POST',headers={
         'Authorization':'Bearer '+os.environ['GH_TOKEN'],
         'Accept':'application/vnd.github+json','Content-Type':'application/json',
         'X-GitHub-Api-Version':'2022-11-28','User-Agent':'ngb-private-ipa-delivery'})
 with urlopen(request,timeout=30) as response:release=json.load(response)
 private_release(release,payload['target_commitish'])
 return release
def anonymous_status(url):
 return subprocess.check_output(['curl','-q','-sS','-L','--max-time','25','-o',os.devnull,'-w','%{http_code}',url],text=True).strip()
def prove_private(release,asset,sha):
 private_release(release,sha)
 if asset.get('state')!='uploaded':raise RuntimeError('Asset upload has not completed')
 if anonymous_status(release['html_url'])!='404' or anonymous_status(asset['browser_download_url'])!='404':
  raise RuntimeError('Anonymous access was not rejected with 404; refusing private binary delivery')
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['prepare','upload','cleanup']);p.add_argument('receipt',type=Path);p.add_argument('--ipa',type=Path);a=p.parse_args()
 repo=os.environ['GITHUB_REPOSITORY'];sha=os.environ['GITHUB_SHA'];run=os.environ['GITHUB_RUN_ID'];attempt=os.environ.get('GITHUB_RUN_ATTEMPT','1');version='4.0.'+os.environ['GITHUB_RUN_NUMBER'];kind='UIProbe' if os.environ.get('NGB_IOS_UI_PROMPT_PROBE')=='1' else 'Release'
 base='repos/'+repo+'/releases';tag='ios-release-'+run+'-'+attempt
 if a.action=='prepare':
  payload={'tag_name':tag,'target_commitish':sha,'name':'Ninja Gaiden Black '+version+' '+kind,'body':('Private optimized UI diagnostic build.' if kind=='UIProbe' else 'Private optimized release with port diagnostics disabled.')+' Keep this release as a draft. Download the IPA asset directly.','draft':True,'prerelease':False,'generate_release_notes':False}
  with tempfile.TemporaryDirectory() as td:
   work=Path(td)
   release=create_private_draft(base,payload)
   receipt={'id':release['id'],'tag':tag,'source':sha,'uploaded':False};a.receipt.write_text(json.dumps(receipt))
   fixture=work/'visibility-fixture.txt';fixture.write_text('Public text used solely to verify unpublished draft asset visibility.\n')
   gh('release','upload',tag,fixture,'--repo',repo)
   release=api(base+'/'+str(release['id']));asset=next(x for x in release['assets'] if x['name']==fixture.name)
   prove_private(release,asset,sha);receipt['fixture_id']=asset['id'];a.receipt.write_text(json.dumps(receipt))
   print('Private draft preflight passed: draft status and anonymous 404 for both page and fixture asset')
 elif a.action=='upload':
  receipt=json.loads(a.receipt.read_text());release=api(base+'/'+str(receipt['id']))
  fixture=next(x for x in release['assets'] if x['id']==receipt['fixture_id']);prove_private(release,fixture,sha)
  assert a.ipa and a.ipa.is_file()
  with tempfile.TemporaryDirectory() as td:
   import shutil
   named=Path(td)/('NinjaGaidenBlack-'+version+'-'+kind+'.ipa');shutil.copy2(a.ipa,named)
   gh('release','upload',receipt['tag'],named,'--repo',repo)
   release=api(base+'/'+str(receipt['id']));asset=next(x for x in release['assets'] if x['name']==named.name)
   try:prove_private(release,asset,sha)
   except Exception:
    gh('api','--method','DELETE',base+'/assets/'+str(asset['id']));raise
   digest=hashlib.sha256(named.read_bytes()).hexdigest()
   if asset['size']!=named.stat().st_size or (asset.get('digest') and asset['digest']!='sha256:'+digest):raise RuntimeError('Uploaded IPA integrity check failed')
   gh('api','--method','DELETE',base+'/assets/'+str(receipt['fixture_id']))
   receipt.update(uploaded=True,url=release['html_url'],asset_url=asset['browser_download_url'],asset_id=asset['id'],sha256=digest,bytes=asset['size'],version=version)
   a.receipt.write_text(json.dumps(receipt,indent=2))
   print('Private IPA draft release ID:',release['id']);print('Private IPA draft page:',release['html_url']);print('Private single IPA URL:',asset['browser_download_url']);print('Private IPA SHA-256:',digest)
 else:
  if not a.receipt.exists():return
  receipt=json.loads(a.receipt.read_text())
  if receipt.get('uploaded'):return
  release=api(base+'/'+str(receipt['id']));private_release(release,sha)
  # Only remove this attempt's empty test draft, never an older release or a delivered IPA.
  if receipt.get('tag')!=tag or any(x['name']!='visibility-fixture.txt' for x in release['assets']):return
  gh('api','--method','DELETE',base+'/'+str(receipt['id']))
if __name__=='__main__':main()
