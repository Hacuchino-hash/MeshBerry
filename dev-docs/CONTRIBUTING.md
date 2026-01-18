# Contributing to MeshBerry

Thank you for your interest in contributing to MeshBerry! This document outlines the contribution process and documentation requirements.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Documentation Requirements](#documentation-requirements)
4. [Development Workflow](#development-workflow)
5. [Pull Request Process](#pull-request-process)
6. [Coding Standards](#coding-standards)
7. [AI Assistants](#ai-assistants)

---

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Help others learn and grow
- Keep discussions technical and productive

---

## Getting Started

### Prerequisites

- PlatformIO with Arduino framework
- VSCode (recommended) or other IDE
- Git
- LILYGO T-Deck or T-Deck Plus hardware (for testing)

### Setup

1. Fork the repository
2. Clone your fork:
   ```bash
   git clone https://github.com/YOUR_USERNAME/MeshBerry.git
   cd MeshBerry
   ```
3. Read the documentation:
   - `dev-docs/REQUIREMENTS.md` - Project requirements
   - `dev-docs/CHANGELOG_INSTRUCTIONS.md` - Documentation requirements
4. Create a feature branch:
   ```bash
   git checkout -b feature/your-feature-name
   ```

---

## Documentation Requirements

### MANDATORY: Every Change Must Be Documented

This is non-negotiable. Every change to the codebase requires:

1. **A changelog entry** in `dev-docs/changes/`
2. **Git commit** with clear message
3. **Updated tests** if applicable

### Creating a Changelog Entry

1. Copy the template:
   ```bash
   cp dev-docs/templates/CHANGE_TEMPLATE.md dev-docs/changes/YYYYMMDD-your-change.md
   ```

2. Fill in ALL required fields:
   - Date
   - Author
   - Type (feature/fix/refactor/docs/test/config)
   - Status
   - Files Modified (complete list)
   - Summary
   - Technical Details
   - Testing
   - Results
   - Breaking Changes

3. See `dev-docs/CHANGELOG_INSTRUCTIONS.md` for full details

### What to Document

| Change Type | Requires Changelog |
|-------------|-------------------|
| New feature | YES |
| Bug fix | YES |
| Refactoring | YES |
| Configuration change | YES |
| Dependency update | YES |
| Documentation update | YES |
| Test addition | YES |
| Build system change | YES |
| Typo fix (code) | YES |
| Typo fix (docs only) | Recommended |

---

## Development Workflow

### Branch Strategy

| Branch | Purpose |
|--------|---------|
| `main` | Production-ready releases |
| `testing` | Integration testing |
| `dev` | Active development |
| `feature/*` | New features |
| `fix/*` | Bug fixes |
| `refactor/*` | Code refactoring |

### Workflow

1. **Start from `dev` branch:**
   ```bash
   git checkout dev
   git pull origin dev
   git checkout -b feature/my-feature
   ```

2. **Make changes and document:**
   - Write code
   - Create changelog entry in `dev-docs/changes/`
   - Write/update tests

3. **Commit with clear messages:**
   ```bash
   git add .
   git commit -m "Add keyboard backlight control feature"
   ```

4. **Push and create PR:**
   ```bash
   git push origin feature/my-feature
   ```

5. **PR targets `dev` branch** (not `main`)

---

## Pull Request Process

### Before Submitting

- [ ] Code compiles without errors
- [ ] All tests pass
- [ ] Changelog entry created in `dev-docs/changes/`
- [ ] Code follows project style guidelines
- [ ] No secrets or credentials in code
- [ ] Documentation updated if needed

### PR Description Template

```markdown
## Summary
[Brief description of changes]

## Changelog Entry
`dev-docs/changes/YYYYMMDD-name.md`

## Type
- [ ] Feature
- [ ] Bug fix
- [ ] Refactor
- [ ] Documentation
- [ ] Test
- [ ] Other: ___

## Testing
[How was this tested?]

## Screenshots (if applicable)
[Add screenshots]

## Checklist
- [ ] Changelog entry created
- [ ] Code compiles
- [ ] Tests pass
- [ ] No breaking changes (or documented)
```

### Review Process

1. At least one maintainer review required
2. All CI checks must pass
3. Changelog entry must be complete
4. No unresolved comments

---

## Coding Standards

### C++ Style

```cpp
// Use descriptive names
void handleKeyboardInput();  // Good
void hki();                  // Bad

// Constants in UPPER_SNAKE_CASE
const int MAX_BUFFER_SIZE = 256;

// Classes in PascalCase
class MeshPacketHandler {
public:
    void processPacket();
private:
    uint8_t buffer[MAX_BUFFER_SIZE];
};

// Variables in camelCase
int messageCount = 0;
String userName = "default";

// Use explicit types
uint8_t packetData[128];  // Good
auto data = getPacket();  // Use sparingly
```

### File Organization

```
src/
├── main.cpp              # Entry point
├── config.h              # Configuration constants
├── drivers/
│   ├── display.cpp/.h    # ST7789 display driver
│   ├── keyboard.cpp/.h   # Keyboard I2C driver
│   ├── lora.cpp/.h       # SX1262 LoRa driver
│   └── gps.cpp/.h        # GPS driver
├── mesh/
│   ├── core.cpp/.h       # MeshCore integration
│   ├── packets.cpp/.h    # Packet handling
│   └── routing.cpp/.h    # Mesh routing
├── ui/
│   ├── screens.cpp/.h    # UI screens
│   ├── widgets.cpp/.h    # UI components
│   └── themes.cpp/.h     # Visual themes
└── utils/
    ├── battery.cpp/.h    # Battery management
    └── storage.cpp/.h    # SD card handling
```

### Comments

```cpp
// Single-line comments for brief explanations

/*
 * Multi-line comments for detailed explanations
 * of complex logic or algorithms.
 */

/**
 * @brief Function documentation
 * @param input Description of parameter
 * @return Description of return value
 */
int processInput(uint8_t input);
```

---

## AI Assistants

### Using AI for Development

AI assistants (Claude, Copilot, Cursor, ChatGPT, etc.) are welcome to contribute, but MUST follow these rules:

1. **Read documentation first:**
   - `dev-docs/REQUIREMENTS.md`
   - `dev-docs/CHANGELOG_INSTRUCTIONS.md`

2. **Document ALL changes:**
   - Create changelog entry for every modification
   - Include AI identifier as author
   - Be thorough in technical details

3. **Handoff protocol:**
   - When session ends, document current state
   - Include what's done, what remains, any blockers
   - Next AI must read recent changelog entries

### AI Author Format

```markdown
| **Author** | Claude (claude-3-opus) |
```

or

```markdown
| **Author** | GitHub Copilot |
```

or

```markdown
| **Author** | Human: [name] with AI assistance |
```

---

## Questions?

- Open an issue for questions
- Check existing issues and changelog entries
- Read `dev-docs/REQUIREMENTS.md` for project context

---

## License

By contributing to MeshBerry, you agree that your contributions will be licensed under the MIT License.
