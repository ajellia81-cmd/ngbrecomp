# 작은 빌드 입력 추출

빌드에는 ISO 안의 `default.xbe`만 필요합니다. 전체 게임 데이터는 iPhone에서 실행할 때 필요합니다.
Drive가 큰 ISO의 반복 다운로드를 제한하면 작은 입력으로 재빌드할 수 있습니다.

`Extract-NGBBuildInput.ps1`을 다운로드 폴더에 저장하고 Windows PowerShell에서 실행합니다.
Windows 기본 PowerShell만 사용하며 프로그램 설치, 관리자 권한, 네트워크 접속은 필요 없습니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\Downloads\Extract-NGBBuildInput.ps1"
```

`-ExecutionPolicy Bypass`는 이 자식 프로세스에만 적용됩니다. 시스템/계정 설정을 저장하지 않습니다.
기본 입력은 다운로드 폴더의 `Ninja Gaiden Black (USA) (En,Ja).iso`입니다.
다른 위치라면 `-IsoPath "실제 ISO 경로"`를 명령 끝에 추가합니다.

결과는 다운로드 폴더의 `NGB-build-input/default.xbe`이며 크기는 **4,571,136바이트**입니다.
원본 ISO는 읽기 전용으로 열고 디스크 디렉터리와 해당 실행 파일만 읽습니다.
원본을 수정하지 않으며 기존 출력도 덮어쓰지 않습니다.

이 작은 파일만 Drive에 올려 링크 뷰어 권한을 주고, 저장소 Actions Secret `NGB_DISC_URL`을
새 링크로 바꿉니다. `NGB_DISC_SHA256`도 따로 설정했다면 새 입력의 해시로 바꾸거나 지웁니다.
CI는 ISO와 단일 XBE를 모두 지원하며 기존 USA 실행 파일 fingerprint로 재검증합니다.
업로드가 끝난 뒤 PC를 켜 둘 필요가 없습니다.

클라우드 PowerShell 7.5.4에서 합성 디스크의 기본 파티션 및 전체 디스크 오프셋을 대상으로
정확한 추출, 원본 불변 및 출력 덮어쓰기 방지를 검증했습니다. 실제 Windows PC에서는 미검증입니다.
`default.xbe`는 비공개 빌드 입력입니다. 공개 저장소에 커밋하지 마세요.
