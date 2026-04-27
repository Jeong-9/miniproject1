# 운전면허 도로주행 시뮬레이터

C++, SFML, ImGui를 활용하여 제작한 2D 운전면허 도로주행/주차 시뮬레이터입니다.

---

## 1. 프로젝트 개요

실제 자동차 면허 시험 규칙을 반영하여  
A코스 시내 도로 주행, B코스 복합 도로 주행, T자 주차, 평행 주차를 구현한 프로젝트입니다.

주행 중 발생하는 신호 위반, 중앙선 침범, 역주행, 안전벨트 미착용 등의 항목을 판정하고  
주행 종료 후 결과 점수판, 상세 결과창, 충돌 상세창, 리플레이 기능을 통해 사용자가 자신의 실수를 확인할 수 있도록 구성했습니다.

---

## 2. 프로젝트 배경

- 실제 면허 시험의 규칙을 게임 형태로 구현해 보고자 함
- 주행 실수와 감점 항목을 시각적으로 확인할 수 있는 학습형 시뮬레이터 제작
- 팀 프로젝트를 통해 맵 제작, 차량 로직, UI, 결과 시스템을 분담하여 구현

---

## 3. 개발 환경

- **Language**: C++
- **IDE**: Visual Studio
- **Graphics Library**: SFML
- **UI Library**: ImGui, ImGui-SFML

---

## 4. 주요 기능

### 시작 화면
- 시간에 따라 배경색이 달라지는 시작 화면
- 코스 선택 및 주차 모드 진입 버튼 구현

### 차량 조작 시스템
- 키보드 입력 기반 가속/감속/조향
- P/N/D/R 기어 상태 반영
- 브레이크, 감속, 회전 처리

### 도로주행 시뮬레이션
- A코스 시내 도로 주행
- B코스 복합 도로 주행
- 교통 신호 및 NPC 차량 반영

### 주차 모드
- T자 주차
- 평행 주차
- 주차 성공 판정 및 시각 효과 제공

### 패널티 판정
- 신호 위반
- 중앙선 침범
- 역주행
- 안전벨트 미착용
- 체크포인트 판정

### 결과 및 리플레이
- 결과 점수판
- 결과 상세창
- 충돌 상세창
- 리플레이 기능

---

## 5. 제가 담당한 기능 (정서현)

저는 **시뮬레이션 종료 후 결과 처리 및 결과 UI 영역**을 담당했습니다.

### 결과 판정 시스템
- `exam_result.h`를 작성하여 감점 로그를 기반으로
  - 실격 여부
  - 총 감점
  - 최종 점수
  - 합격/불합격/실격 상태
  를 계산하는 구조를 정리했습니다.

### 결과 데이터 생성 및 전달
- `main.cpp`에서 evaluator의 감점 로그를 가져와 `buildExamResult()`를 호출하고,
- 계산된 결과를 결과창과 상세 결과창으로 전달하도록 흐름을 연결했습니다.

### 결과 화면 구현
- 결과 점수판 구현
- 결과 상세창 구현
- 충돌 상세창 구현
- 감점 내역과 발생 시간을 상세하게 확인할 수 있도록 구성

### UI 개선
- 결과 화면 한글화
- 텍스트 정렬, 버튼 배치, 창 크기 조정
- 사용자가 결과를 직관적으로 이해할 수 있도록 화면 가독성 개선

---

## 6. 팀원 역할

- **엄재니**: 코드 병합, 메인 루프 제작, 시뮬레이션 차량의 물리 엔진 설계, 프로젝트 공용 타입 설계
- **송현우**: 코드 병합,모든 화면 렌더링 제작
- **이승민**: 신호등 및 패널티 로직, 감점 및 체크포인트 판정 설계, 리플레이 기능 제작
- **김승영**: A/B코스 맵 제작, T자/평행 주차 맵 제작, NPC 자동차들의 로직 설계 및 제작
- **정서현**: 결과 판정 시스템, 시뮬레이션 완료 시의 점수판 및 메뉴 제작, 한글화 및 UI 개선
- **정지수**: 키보드 입력 기반 차량 조작 상태 관리, 메인 화면 디자인

---

## 7. 스크린샷

### 시작 화면
<img width="793" height="368" alt="image" src="https://github.com/user-attachments/assets/7c855de8-7e7d-45f8-a39d-67f2a31b40a7" />


### A코스 주행 화면
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/42925658-fde1-4036-9c67-ecebd8bf4b81" />

### B코스 주행 화면
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/06760470-9de9-41fc-8857-1c02ff51574d" />


### T자 주차 / 평행 주차
<img width="1918" height="1073" alt="image" src="https://github.com/user-attachments/assets/af9a31de-c0d7-46af-a5a9-ad980bf2a922" />

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/6bd9c07e-9b73-43f7-bf1e-bdac6b6f06c5" />

### 결과 창
<img width="959" height="431" alt="image" src="https://github.com/user-attachments/assets/5ba6b132-4930-463a-a853-bce5e2f03b15" />

### 결과 상세창
<img width="1067" height="342" alt="image" src="https://github.com/user-attachments/assets/5b2d08eb-8f61-4b11-881e-f0e10f31f817" />

### 충돌 창
<img width="1067" height="338" alt="image" src="https://github.com/user-attachments/assets/57d2bfe8-bbce-4412-96b4-d67ae2efc87d" />

### 충돌 상세창
<img width="1063" height="340" alt="image" src="https://github.com/user-attachments/assets/19b4fe7f-c8cc-4ab3-b969-4a71687d8560" />




---

## 8. 실행 방법

1. Visual Studio에서 프로젝트 실행
2. 필요한 SFML / ImGui-SFML 라이브러리 설정
3. 실행 후 코스 선택
4. 도로주행 / 주차 모드 플레이

---

## 9. 시연 영상
https://youtu.be/G-jD45zPmvU
