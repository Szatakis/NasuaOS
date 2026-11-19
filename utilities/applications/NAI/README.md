# NAI (NasuaAI)

NAI is an offline chat application for NasuaOS. It is implemented in C++ using
the existing NAPP API and does not download or bundle a model, knowledge files,
or additional libraries.

The current assistant uses hand-written rules and built-in responses. It can
answer selected questions about NasuaOS, building the project, and basic
computer concepts. It is not a large language model and cannot answer arbitrary
questions or fetch current information. Unknown questions receive an explicit
fallback response.

Build this application from the repository root with:

```sh
make -C utilities/applications/NAI -f ../app.mk APP=NAI
```

The applications build discovers this directory automatically. It produces
`NAI.napp`, displayed to users as **NasuaAI**.
