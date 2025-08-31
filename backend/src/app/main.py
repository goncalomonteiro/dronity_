from fastapi import FastAPI


def create_app() -> FastAPI:
    app = FastAPI(title="Verity Backend", version="0.0.1")

    @app.get("/health")
    def health() -> dict:
        return {"ok": True}

    return app


app = create_app()

