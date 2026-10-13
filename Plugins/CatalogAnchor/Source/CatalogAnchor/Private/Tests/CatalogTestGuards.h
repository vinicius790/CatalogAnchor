// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CatalogTestGuards
{
	/**
	 * Testes que criam GameInstance/mundo próprios não podem rodar com PIE ativo: UGameInstance::Init mexe
	 * em FNetDelegates globais que o PIE já usa. Avisa e devolve true: o teste deve fazer `return true`.
	 * GEditor->PlayWorld exigiria o módulo UnrealEd; o contexto de mundo PIE vale em qualquer build.
	 */
	inline bool ShouldSkipDuringPIE(FAutomationTestBase& Test)
	{
		bool bPIE = GIsPlayInEditorWorld;
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				bPIE |= Context.WorldType == EWorldType::PIE;
			}
		}
		if (bPIE)
		{
			Test.AddWarning(TEXT("Teste de mundo ignorado: PIE ativo (feche o Play In Editor e rode de novo)."));
		}
		return bPIE;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
