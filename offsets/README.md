# 설치본에서 확인한 오프셋

Steam build **19307283**, Windows x86. 기본 실행은 `hl2_complete/bin`의 DLL을 사용합니다. `hl2/bin`은 별도의 DLL 조합이며 함수 RVA가 다릅니다. 실행 시 두 조합을 자동 구분합니다. 각각의 SHA-256은 `build-19307283-complete.json`과 `build-19307283.json`에 기록되어 있습니다.

RVA는 해당 모듈의 로드 기준 주소에 더합니다. 엔티티 필드는 서버와 클라이언트 구분을 지켜야 합니다.

| 서버 함수 RVA | 기본 `hl2_complete` | `hl2` |
|---|---:|---:|
| CBaseEntity::FireBullets | `0x114C60` | `0x114830` |
| CServerGameDLL::GameFrame | `0x188050` | `0x187D60` |
| CBasePlayer::SnapEyeAngles | `0x207F60` | `0x207A50` |
| .357 반동 호출의 복귀 주소 | `0x40E90D` | `0x40E95D` |
| CHL2_Player::GetDataDescMap | `0x2E4800` | `0x2E43D0` |

아래 엔티티 필드는 두 조합에서 같음을 별도 프로세스 진단으로 확인했습니다.

| 대상 | 값 | 확인 방법 |
|---|---:|---|
| 서버 엔티티 `m_iHealth` | `0xE0` | 격리 프로세스 데이터맵 진단 |
| 서버 엔티티 `m_lifeState` | `0xE4` | 격리 프로세스 데이터맵 진단 |
| 서버 플레이어 `m_Local.m_vecPunchAngle` | `0x8CC` | 격리 프로세스 데이터맵 진단 |
| 서버 플레이어 `m_Local.m_vecPunchAngleVel` | `0x8D8` | 격리 프로세스 데이터맵 진단 |
| 클라이언트 플레이어 `m_vecPunchAngle[0]` | `0xEB8` | 실제 RecvTable 탐색 |
| 클라이언트 플레이어 `m_vecPunchAngleVel[0]` | `0xEF4` | 실제 RecvTable 탐색 |
| 서버 엔티티 `m_pStudioHdr` | `0x44C` | 모델/뼈 함수 디스어셈블 |
| 서버 엔티티 `m_vecAbsOrigin` | `0x27C` | 설치본 코드와 공식 SDK 의미 대조 |
| 서버 엔티티 `m_Collision` | `0x148` | GetCollideable 함수의 `lea` 피연산자 |
| 서버 엔티티 `m_nHitboxSet` | `0x358` | 데이터맵 필드 기술자 |

| 가상 함수 | 슬롯 번호 (0부터 시작) |
|---|---:|
| CBaseEntity::GetDataDescMap | 11 |
| CBaseEntity::FireBullets | 112 |
| CBaseAnimating::GetBoneTransform | 198 |
| CBaseCombatCharacter::IRelationType | 296 |
| IServerGameDLL::GameFrame | 5 |
| IInputSystem::EnableInput | 7 |
| IInputSystem::ResetInputState | 25 |
| VGUI Surface030::UnlockCursor / LockCursor | 61 / 62 |

코드는 체력·생존 상태·반동 필드를 실행 시 이름으로 다시 찾습니다. 이 표의 필드 값을 그대로 하드코딩해 읽는 방식은 사용하지 않습니다. 모델과 사격 코드에 필요한 고정 값은 DLL 지문이 맞을 때만 사용합니다.

진단 프로그램은 게임 DLL을 자체 프로세스에 로드해 검사했습니다. 두 프로필 모두 실제 초기화와 게임 훅 설치 후 `ready` 상태를 확인했습니다. 플레이 중인 맵에서의 사격 정확도와 NPC별 동작 검증은 남아 있습니다. `build/abi-complete.txt`, `build/abi-legacy.txt`에 진단 결과가 있습니다.

인터넷 참고 자료는 [공식 SDK](https://github.com/ValveSoftware/source-sdk-2013/tree/singleplayer), [SourcePauseTool](https://github.com/OutOfBoundsOffice/SourcePauseTool), [구버전 공개 예제](https://github.com/p410n3/hl2hop)입니다. 위 숫자는 설치본 분석 및 진단 결과입니다.
